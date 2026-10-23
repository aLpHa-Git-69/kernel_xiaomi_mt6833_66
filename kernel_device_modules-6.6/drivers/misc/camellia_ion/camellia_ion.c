// SPDX-License-Identifier: GPL-2.0
/*
 * camellia: /dev/ion for the stock (4.14-era) vendor userspace on 6.6.
 *
 * camellia's MIUI 14 vendor ran on MediaTek's 4.14 ION (handle based, with
 * MediaTek custom ioctls), and its ueventd only grants /dev/ion. 6.6 has no
 * ION; buffers come from dma-heaps. This driver serves both kinds of ION
 * client from the 6.6 dma-heaps:
 *
 *  - modern ION (libion ion_alloc_fd / ION_IOC_HEAP_QUERY), which is what
 *    libdmabufheap falls back to for "mtk_mm" (codec2, gralloc), since the
 *    mtk_mm dma-heaps have no device node (see dma-heap.c);
 *  - MediaTek legacy ION (handles: ALLOC/FREE/SHARE/MAP/IMPORT/SYNC and the
 *    ION_CMD_SYSTEM / ION_CMD_MULTIMEDIA custom ioctls), which vpud's codec
 *    libraries use to turn buffer fds into IOVAs for the codec hardware.
 *
 * libion decides "legacy or modern" from ION_IOC_FREE on handle 0 (-ENOTTY
 * means modern). A file becomes legacy once it issues a MediaTek custom ioctl:
 * mt_ion_open() (libion_mtk) sets the client name right after open, before
 * any legacy call; libdmabufheap never sends custom ioctls.
 */

#include <linux/compat.h>
#include <linux/cred.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* ---- 4.14 MediaTek ION uapi (drivers/staging/android/uapi/ion.h) ---- */

#define ION_IOC_MAGIC		'I'
#define ION_HEAP_TYPE_SYSTEM	0
#define ION_FLAG_CACHED		1
#define ION_NUM_HEAP_IDS	32
#define MAX_HEAP_NAME		32

/* legacy alloc: size_t len, align; heap_id_mask, flags; int handle */
struct ion_allocation_data_legacy {
	__u64 len;
	__u64 align;
	__u32 heap_id_mask;
	__u32 flags;
	__s32 handle;
	__u32 pad;
};

struct ion_allocation_data_legacy32 {
	__u32 len;
	__u32 align;
	__u32 heap_id_mask;
	__u32 flags;
	__s32 handle;
};

/* modern (4.12+) alloc */
struct ion_allocation_data {
	__u64 len;
	__u32 heap_id_mask;
	__u32 flags;
	__u32 fd;
	__u32 unused;
};

struct ion_fd_data {
	__s32 handle;
	__s32 fd;
};

struct ion_handle_data {
	__s32 handle;
};

struct ion_custom_data {
	__u32 cmd;
	__u32 pad;
	__u64 arg;
};

struct ion_custom_data32 {
	__u32 cmd;
	__u32 arg;
};

struct ion_heap_data {
	char name[MAX_HEAP_NAME];
	__u32 type;
	__u32 heap_id;
	__u32 reserved0;
	__u32 reserved1;
	__u32 reserved2;
};

struct ion_heap_query {
	__u32 cnt;
	__u32 reserved0;
	__u64 heaps;
	__u32 reserved1;
	__u32 reserved2;
};

#define ION_IOC_ALLOC_LEGACY	_IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data_legacy)
#define ION_IOC_ALLOC_LEGACY32	_IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data_legacy32)
#define ION_IOC_ALLOC		_IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data)
#define ION_IOC_FREE		_IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)
#define ION_IOC_MAP		_IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)
#define ION_IOC_SHARE		_IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)
#define ION_IOC_IMPORT		_IOWR(ION_IOC_MAGIC, 5, struct ion_fd_data)
#define ION_IOC_CUSTOM		_IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)
#define ION_IOC_CUSTOM32	_IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data32)
#define ION_IOC_SYNC		_IOWR(ION_IOC_MAGIC, 7, struct ion_fd_data)
#define ION_IOC_HEAP_QUERY	_IOWR(ION_IOC_MAGIC, 8, struct ion_heap_query)

/* ---- 4.14 MediaTek custom ioctls (ion/mtk/ion_drv.h, mtk_ion.h) ---- */

enum { ION_CMD_SYSTEM, ION_CMD_MULTIMEDIA, ION_CMD_MULTIMEDIA_SEC };

enum {
	ION_MM_CONFIG_BUFFER,
	ION_MM_SET_DEBUG_INFO,
	ION_MM_GET_DEBUG_INFO,
	ION_MM_SET_SF_BUF_INFO,
	ION_MM_GET_SF_BUF_INFO,
	ION_MM_CONFIG_BUFFER_EXT,
	ION_MM_ACQ_CACHE_POOL,
	ION_MM_QRY_CACHE_POOL,
	ION_MM_GET_IOVA,
	ION_MM_GET_IOVA_EXT,
};

enum {
	ION_SYS_CACHE_SYNC,
	ION_SYS_GET_PHYS,
	ION_SYS_GET_CLIENT,
	ION_SYS_SET_HANDLE_BACKTRACE,
	ION_SYS_SET_CLIENT_NAME,
	ION_SYS_DMA_OP,
};

enum {
	ION_CACHE_CLEAN_BY_RANGE,
	ION_CACHE_INVALID_BY_RANGE,
	ION_CACHE_FLUSH_BY_RANGE,
	ION_CACHE_CLEAN_BY_RANGE_USE_PA,
	ION_CACHE_INVALID_BY_RANGE_USE_PA,
	ION_CACHE_FLUSH_BY_RANGE_USE_PA,
	ION_CACHE_CLEAN_ALL,
	ION_CACHE_INVALID_ALL,
	ION_CACHE_FLUSH_ALL,
};

enum {
	ION_DMA_MAP_AREA,
	ION_DMA_UNMAP_AREA,
	ION_DMA_MAP_AREA_VA,
	ION_DMA_UNMAP_AREA_VA,
	ION_DMA_FLUSH_BY_RANGE,
	ION_DMA_FLUSH_BY_RANGE_USE_VA,
	ION_DMA_CACHE_FLUSH_ALL,
};

/* heap ids (mtk_ion.h enum mtk_ion_heap_type) */
#define ION_HEAP_TYPE_MULTIMEDIA		10
#define ION_HEAP_TYPE_FB			11
#define ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA	12
#define ION_HEAP_TYPE_MULTIMEDIA_2D_FR		17
#define ION_MM_DBG_NAME_LEN			48

/*
 * The custom structs hold "union { int handle; struct ion_handle *kernel_handle; }"
 * and longs, so 64- and 32-bit userspace lay them out differently; both
 * ion_mm_data variants are 80 bytes. Offsets checked against vpud's 32-bit
 * libvpud_vcodec (handle at 8, module_id at 12 of an 80-byte ion_mm_data).
 */
struct ion_mm_data64 {
	__u32 mm_cmd;
	__u32 pad;
	union {
		struct {
			__s32 handle;
			__u32 handle_hi;
			__s32 module_id;
			__u32 security;
			__u32 coherent;
			__u32 reserve_iova_start;
			__u32 reserve_iova_end;
			__u32 pad;
			__u64 phy_addr;		/* get_iova only */
			__u64 len;		/* get_iova only */
		} iova;
		struct {
			__s32 handle;
			__u32 handle_hi;
			char dbg_name[ION_MM_DBG_NAME_LEN];
			__u32 value[4];
		} dbg;
		__u8 raw[72];
	};
};

struct ion_mm_data32 {
	__u32 mm_cmd;
	__u32 pad;
	union {
		struct {
			__s32 handle;
			__s32 module_id;
			__u32 security;
			__u32 coherent;
			__u32 reserve_iova_start;
			__u32 reserve_iova_end;
			__u64 phy_addr;		/* get_iova only */
			__u32 len;		/* get_iova only */
			__u32 pad;
		} iova;
		struct {
			__s32 handle;
			char dbg_name[ION_MM_DBG_NAME_LEN];
			__u32 value[4];
		} dbg;
		__u8 raw[72];
	};
};

struct ion_sys_data64 {
	__u32 sys_cmd;
	__u32 pad;
	union {
		struct {
			__s32 handle;
			__u32 handle_hi;
			__u64 va;
			__u32 size;
			__u32 sync_type;
			__u64 iova;
		} cache_sync;
		struct {
			__s32 handle;
			__u32 handle_hi;
			__u64 phy_addr;
			__u64 len;
		} get_phys;
		struct {
			__s32 handle;
			__u32 handle_hi;
			__u64 va;
			__u32 size;
			__u32 dma_type;
			__u32 dma_dir;
		} dma;
		__u32 client;
		char client_name[ION_MM_DBG_NAME_LEN];
		__u8 raw[112];
	};
};

struct ion_sys_data32 {
	__u32 sys_cmd;
	__u32 pad;
	union {
		struct {
			__s32 handle;
			__u32 va;
			__u32 size;
			__u32 sync_type;
			__u64 iova;
		} cache_sync;
		struct {
			__s32 handle;
			__u32 phy_addr;
			__u32 len;
		} get_phys;
		struct {
			__s32 handle;
			__u32 va;
			__u32 size;
			__u32 dma_type;
			__u32 dma_dir;
		} dma;
		__u32 client;
		char client_name[ION_MM_DBG_NAME_LEN];
		__u8 raw[88];
	};
};

/* ---- heaps ---- */

struct cion_heap {
	const char *name;
	u32 heap_id;
	u32 type;
};

/* what modern ION reports: libdmabufheap looks heaps up by name */
static const struct cion_heap cion_heaps[] = {
	{ "system",		ION_HEAP_TYPE_SYSTEM,		ION_HEAP_TYPE_SYSTEM },
	{ "mtk_mm",		ION_HEAP_TYPE_MULTIMEDIA,	ION_HEAP_TYPE_MULTIMEDIA },
	{ "mtk_mm-uncached",	ION_HEAP_TYPE_MULTIMEDIA_2D_FR,	ION_HEAP_TYPE_MULTIMEDIA },
};

static const char *cion_modern_heap(u32 heap_id_mask)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cion_heaps); i++)
		if (heap_id_mask & BIT(cion_heaps[i].heap_id))
			return cion_heaps[i].name;
	return NULL;
}

/* 4.14 heap masks: multimedia / fb / camera are the MediaTek mm heap */
static const char *cion_legacy_heap(u32 heap_id_mask, u32 flags)
{
	bool cached = flags & ION_FLAG_CACHED;

	if (heap_id_mask & (BIT(ION_HEAP_TYPE_MULTIMEDIA) | BIT(ION_HEAP_TYPE_FB) |
			    BIT(ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA)))
		return cached ? "mtk_mm" : "mtk_mm-uncached";
	if (heap_id_mask & BIT(ION_HEAP_TYPE_SYSTEM))
		return cached ? "system" : "system-uncached";
	return NULL;	/* secure heaps: not supported */
}

static struct dma_buf *cion_alloc(const char *heap_name, size_t len)
{
	struct dma_heap *heap;
	struct dma_buf *buf;

	if (!heap_name || !len)
		return ERR_PTR(-EINVAL);
	heap = dma_heap_find(heap_name);
	if (!heap)
		return ERR_PTR(-ENODEV);
	buf = dma_heap_buffer_alloc(heap, PAGE_ALIGN(len), O_RDWR | O_CLOEXEC, 0);
	dma_heap_put(heap);
	return buf;
}

/* ---- clients and legacy handles ---- */

struct cion_client {
	struct mutex lock;
	struct idr handles;
	bool legacy;
	char name[ION_MM_DBG_NAME_LEN];
};

enum cion_dom { CION_VDEC, CION_VENC, CION_MDP, CION_DISP, CION_NR_DOM };

struct cion_handle {
	struct dma_buf *buf;
	int refs;
	int module_id;
	/* one mapping per IOMMU domain: vdec/venc 4G~8G, MDP 8G~12G, disp 0~4G */
	struct dma_buf_attachment *att[CION_NR_DOM];
	struct sg_table *sgt[CION_NR_DOM];
};

static struct device *cion_dom_dev[CION_NR_DOM];
static DEFINE_MUTEX(cion_dev_lock);

/*
 * 4.14 M4U port ids are (larb << 5) | port. The buffer has to be mapped for a
 * device in the IOMMU domain the port's larb reads from (its BANK_SEL gives
 * the upper IOVA bits): larb 7 = venc, larb 2 = MDP (DpFramework in the
 * composer), larbs 0/1 = display; everything else stays on vdec.
 */
static enum cion_dom cion_port_dom(int module_id)
{
	switch (module_id >> 5) {
	case 7:
		return CION_VENC;
	case 2:
		return CION_MDP;
	case 0:
	case 1:
		return CION_DISP;
	default:
		return CION_VDEC;
	}
}

static struct device *cion_dom_device(enum cion_dom dom)
{
	static const char * const compat[CION_NR_DOM] = {
		[CION_VDEC] = "mediatek,mt6833-vcodec-dec",
		[CION_VENC] = "mediatek,mt6833-vcodec-enc",
		[CION_MDP] = "mediatek,mdpsys_config",
		[CION_DISP] = "mediatek,mt6833-disp",
	};
	struct device_node *np;
	struct platform_device *pdev;

	mutex_lock(&cion_dev_lock);
	if (!cion_dom_dev[dom]) {
		np = of_find_compatible_node(NULL, NULL, compat[dom]);
		pdev = np ? of_find_device_by_node(np) : NULL;
		of_node_put(np);
		if (pdev)
			cion_dom_dev[dom] = &pdev->dev;
	}
	mutex_unlock(&cion_dev_lock);
	return cion_dom_dev[dom];
}

static int cion_handle_new(struct cion_client *c, struct dma_buf *buf)
{
	struct cion_handle *h;
	int id;

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return -ENOMEM;
	h->buf = buf;
	h->refs = 1;
	/* handle 0 is libion's legacy/modern probe: start at 1 */
	id = idr_alloc(&c->handles, h, 1, 0, GFP_KERNEL);
	if (id < 0)
		kfree(h);
	return id;
}

static void cion_handle_destroy(struct cion_handle *h)
{
	int i;

	for (i = 0; i < CION_NR_DOM; i++) {
		if (h->sgt[i])
			dma_buf_unmap_attachment_unlocked(h->att[i], h->sgt[i], DMA_BIDIRECTIONAL);
		if (h->att[i])
			dma_buf_detach(h->buf, h->att[i]);
	}
	dma_buf_put(h->buf);
	kfree(h);
}

/* IOVA of a handle in the port's IOMMU domain; mapped once per domain, kept until freed */
static int cion_handle_iova(struct cion_handle *h, int module_id, dma_addr_t *iova)
{
	enum cion_dom dom = cion_port_dom(module_id ? module_id : h->module_id);
	struct dma_buf_attachment *att;
	struct sg_table *sgt;
	struct device *dev;

	if (!h->sgt[dom]) {
		dev = cion_dom_device(dom);
		if (!dev)
			return -ENODEV;
		att = dma_buf_attach(h->buf, dev);
		if (IS_ERR(att))
			return PTR_ERR(att);
		sgt = dma_buf_map_attachment_unlocked(att, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			dma_buf_detach(h->buf, att);
			return PTR_ERR(sgt);
		}
		h->att[dom] = att;
		h->sgt[dom] = sgt;
	}
	*iova = sg_dma_address(h->sgt[dom]->sgl);
	return 0;
}

static void cion_sync(struct dma_buf *buf, bool clean, bool invalidate)
{
	if (invalidate)
		dma_buf_begin_cpu_access(buf, DMA_FROM_DEVICE);
	if (clean)
		dma_buf_end_cpu_access(buf, DMA_TO_DEVICE);
}

/* ---- legacy ioctls (client lock held) ---- */

static long cion_legacy_alloc(struct cion_client *c, size_t len, u32 mask,
			      u32 flags, s32 *handle)
{
	struct dma_buf *buf = cion_alloc(cion_legacy_heap(mask, flags), len);
	int id;

	if (IS_ERR(buf))
		return PTR_ERR(buf);
	id = cion_handle_new(c, buf);
	if (id < 0) {
		dma_buf_put(buf);
		return id;
	}
	*handle = id;
	return 0;
}

static long cion_free(struct cion_client *c, s32 id)
{
	struct cion_handle *h;

	if (!c->legacy)
		return -ENOTTY;		/* modern ION: no FREE */
	h = idr_find(&c->handles, id);
	if (!h)
		return -EINVAL;
	if (--h->refs == 0) {
		idr_remove(&c->handles, id);
		cion_handle_destroy(h);
	}
	return 0;
}

/*
 * 4.14 ION exported a new dma-buf on every share, so the fd was always the
 * caller's own file. vpud imports buffers the Codec2 HAL allocated and maps
 * them through SHARE/MAP; SELinux does not let it use the HAL's file, so hand
 * out a view of the dma-buf opened with the caller's credentials.
 */
static long cion_share(struct cion_client *c, struct ion_fd_data *d)
{
	struct cion_handle *h = idr_find(&c->handles, d->handle);
	struct file *file;
	int fd;

	if (!h)
		return -EINVAL;
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return fd;
	file = camellia_dma_buf_view(h->buf, current_cred());
	if (IS_ERR(file)) {
		put_unused_fd(fd);
		return PTR_ERR(file);
	}
	fd_install(fd, file);
	d->fd = fd;
	return 0;
}

static int cion_find_buf(int id, void *p, void *data)
{
	return ((struct cion_handle *)p)->buf == data ? id : 0;
}

static long cion_import(struct cion_client *c, struct ion_fd_data *d)
{
	struct dma_buf *buf = dma_buf_get(d->fd);
	struct cion_handle *h;
	int id;

	if (IS_ERR(buf))
		return PTR_ERR(buf);
	/* ION gives back the same handle for a buffer the client already has */
	id = idr_for_each(&c->handles, cion_find_buf, buf);
	if (id > 0) {
		h = idr_find(&c->handles, id);
		h->refs++;
		dma_buf_put(buf);
	} else {
		id = cion_handle_new(c, buf);
		if (id < 0) {
			dma_buf_put(buf);
			return id;
		}
	}
	d->handle = id;
	return 0;
}

static long cion_sync_fd(int fd)
{
	struct dma_buf *buf = dma_buf_get(fd);

	if (IS_ERR(buf))
		return PTR_ERR(buf);
	cion_sync(buf, true, true);
	dma_buf_put(buf);
	return 0;
}

/* ---- MediaTek custom ioctls ---- */

static long cion_mm(struct cion_client *c, void __user *uarg, bool compat)
{
	union {
		struct ion_mm_data64 d64;
		struct ion_mm_data32 d32;
	} u;
	struct cion_handle *h;
	dma_addr_t iova;
	s32 handle, module_id;
	u32 cmd;
	long ret = 0;

	BUILD_BUG_ON(sizeof(struct ion_mm_data64) != 80);
	BUILD_BUG_ON(sizeof(struct ion_mm_data32) != 80);
	if (copy_from_user(&u, uarg, 80))
		return -EFAULT;
	cmd = u.d64.mm_cmd;
	handle = compat ? u.d32.iova.handle : u.d64.iova.handle;
	module_id = compat ? u.d32.iova.module_id : u.d64.iova.module_id;

	switch (cmd) {
	case ION_MM_CONFIG_BUFFER:
	case ION_MM_CONFIG_BUFFER_EXT:
		h = idr_find(&c->handles, handle);
		if (!h)
			return -EINVAL;
		h->module_id = module_id;
		return 0;
	case ION_MM_GET_IOVA:
	case ION_MM_GET_IOVA_EXT:
		h = idr_find(&c->handles, handle);
		if (!h)
			return -EINVAL;
		if (!h->module_id)
			h->module_id = module_id;
		ret = cion_handle_iova(h, module_id, &iova);
		if (ret)
			return ret;
		if (compat) {
			u.d32.iova.phy_addr = iova;
			u.d32.iova.len = h->buf->size;
		} else {
			u.d64.iova.phy_addr = iova;
			u.d64.iova.len = h->buf->size;
		}
		break;
	case ION_MM_SET_DEBUG_INFO:
	case ION_MM_SET_SF_BUF_INFO:
		return 0;
	case ION_MM_GET_DEBUG_INFO:
	case ION_MM_GET_SF_BUF_INFO:
		/* not kept: report an empty record */
		if (compat)
			memset(u.d32.raw + sizeof(s32), 0, sizeof(u.d32.raw) - sizeof(s32));
		else
			memset(u.d64.raw + sizeof(u64), 0, sizeof(u.d64.raw) - sizeof(u64));
		break;
	default:
		pr_info("camellia_ion: %s: unsupported mm cmd %u\n", c->name, cmd);
		return -EINVAL;
	}
	return copy_to_user(uarg, &u, 80) ? -EFAULT : 0;
}

static long cion_sys(struct cion_client *c, void __user *uarg, bool compat)
{
	union {
		struct ion_sys_data64 d64;
		struct ion_sys_data32 d32;
	} u;
	size_t size = compat ? sizeof(u.d32) : sizeof(u.d64);
	struct cion_handle *h;
	dma_addr_t iova;
	s32 handle;
	u32 cmd, type;
	long ret;

	BUILD_BUG_ON(sizeof(struct ion_sys_data32) != 96);
	BUILD_BUG_ON(sizeof(struct ion_sys_data64) != 120);
	if (copy_from_user(&u, uarg, size))
		return -EFAULT;
	cmd = u.d64.sys_cmd;

	switch (cmd) {
	case ION_SYS_SET_CLIENT_NAME:
		memcpy(c->name, compat ? u.d32.client_name : u.d64.client_name,
		       ION_MM_DBG_NAME_LEN);
		c->name[ION_MM_DBG_NAME_LEN - 1] = '\0';
		return 0;
	case ION_SYS_GET_CLIENT:
		if (compat)
			u.d32.client = 0;
		else
			u.d64.client = 0;
		break;
	case ION_SYS_SET_HANDLE_BACKTRACE:
		return 0;
	case ION_SYS_GET_PHYS:
		/* 4.14: for multimedia buffers this is the MVA (IOVA) */
		handle = compat ? u.d32.get_phys.handle : u.d64.get_phys.handle;
		h = idr_find(&c->handles, handle);
		if (!h)
			return -EINVAL;
		ret = cion_handle_iova(h, 0, &iova);
		if (ret)
			return ret;
		if (compat) {
			u.d32.get_phys.phy_addr = iova;
			u.d32.get_phys.len = h->buf->size;
		} else {
			u.d64.get_phys.phy_addr = iova;
			u.d64.get_phys.len = h->buf->size;
		}
		break;
	case ION_SYS_CACHE_SYNC:
		handle = compat ? u.d32.cache_sync.handle : u.d64.cache_sync.handle;
		type = compat ? u.d32.cache_sync.sync_type : u.d64.cache_sync.sync_type;
		h = idr_find(&c->handles, handle);
		if (!h)
			return -EINVAL;
		switch (type % 3) {	/* clean / invalid / flush, by range or all */
		case 0:
			cion_sync(h->buf, true, false);
			break;
		case 1:
			cion_sync(h->buf, false, true);
			break;
		default:
			cion_sync(h->buf, true, true);
			break;
		}
		return 0;
	case ION_SYS_DMA_OP:
		handle = compat ? u.d32.dma.handle : u.d64.dma.handle;
		type = compat ? u.d32.dma.dma_type : u.d64.dma.dma_type;
		h = idr_find(&c->handles, handle);
		if (!h)
			return -EINVAL;
		if (type == ION_DMA_MAP_AREA || type == ION_DMA_MAP_AREA_VA)
			cion_sync(h->buf, true, false);
		else if (type == ION_DMA_UNMAP_AREA || type == ION_DMA_UNMAP_AREA_VA)
			cion_sync(h->buf, false, true);
		else
			cion_sync(h->buf, true, true);
		return 0;
	default:
		pr_info("camellia_ion: %s: unsupported sys cmd %u\n", c->name, cmd);
		return -EINVAL;
	}
	return copy_to_user(uarg, &u, size) ? -EFAULT : 0;
}

static long cion_custom(struct cion_client *c, u32 cmd, void __user *arg, bool compat)
{
	/* only MediaTek's libion_mtk sends these: this is a legacy client */
	c->legacy = true;
	switch (cmd) {
	case ION_CMD_SYSTEM:
		return cion_sys(c, arg, compat);
	case ION_CMD_MULTIMEDIA:
		return cion_mm(c, arg, compat);
	default:
		return -EINVAL;	/* ION_CMD_MULTIMEDIA_SEC: no secure heaps */
	}
}

/* ---- modern ioctls ---- */

static long cion_heap_query(struct ion_heap_query *q)
{
	struct ion_heap_data hd;
	u32 i, n = 0;

	if (!q->heaps) {
		q->cnt = ARRAY_SIZE(cion_heaps);
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(cion_heaps) && n < q->cnt; i++) {
		struct dma_heap *heap = dma_heap_find(cion_heaps[i].name);

		if (!heap)
			continue;
		dma_heap_put(heap);
		memset(&hd, 0, sizeof(hd));
		strscpy(hd.name, cion_heaps[i].name, sizeof(hd.name));
		hd.type = cion_heaps[i].type;
		hd.heap_id = cion_heaps[i].heap_id;
		if (copy_to_user(u64_to_user_ptr(q->heaps + n * sizeof(hd)), &hd, sizeof(hd)))
			return -EFAULT;
		n++;
	}
	q->cnt = n;
	return 0;
}

static long cion_modern_alloc(struct ion_allocation_data *d)
{
	struct dma_buf *buf = cion_alloc(cion_modern_heap(d->heap_id_mask), d->len);
	int fd;

	if (IS_ERR(buf))
		return PTR_ERR(buf);
	fd = dma_buf_fd(buf, O_CLOEXEC);
	if (fd < 0) {
		dma_buf_put(buf);
		return fd;
	}
	d->fd = fd;
	return 0;
}

/* ---- file ops ---- */

static long cion_ioctl_common(struct file *file, unsigned int cmd,
			      unsigned long arg, bool compat)
{
	struct cion_client *c = file->private_data;
	void __user *uarg = (void __user *)arg;
	union {
		struct ion_allocation_data alloc;
		struct ion_allocation_data_legacy alloc_l;
		struct ion_allocation_data_legacy32 alloc_l32;
		struct ion_fd_data fd;
		struct ion_handle_data handle;
		struct ion_custom_data custom;
		struct ion_custom_data32 custom32;
		struct ion_heap_query query;
	} d;
	s32 handle;
	long ret;

	if (_IOC_SIZE(cmd) > sizeof(d))
		return -EINVAL;
	if (copy_from_user(&d, uarg, _IOC_SIZE(cmd)))
		return -EFAULT;

	mutex_lock(&c->lock);
	switch (cmd) {
	case ION_IOC_ALLOC:
		ret = cion_modern_alloc(&d.alloc);
		break;
	case ION_IOC_ALLOC_LEGACY:
		c->legacy = true;
		ret = cion_legacy_alloc(c, d.alloc_l.len, d.alloc_l.heap_id_mask,
					d.alloc_l.flags, &handle);
		d.alloc_l.handle = handle;
		break;
	case ION_IOC_ALLOC_LEGACY32:
		c->legacy = true;
		ret = cion_legacy_alloc(c, d.alloc_l32.len, d.alloc_l32.heap_id_mask,
					d.alloc_l32.flags, &handle);
		d.alloc_l32.handle = handle;
		break;
	case ION_IOC_FREE:
		ret = cion_free(c, d.handle.handle);
		break;
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
		ret = cion_share(c, &d.fd);
		break;
	case ION_IOC_IMPORT:
		ret = cion_import(c, &d.fd);
		break;
	case ION_IOC_SYNC:
		ret = cion_sync_fd(d.fd.fd);
		break;
	case ION_IOC_CUSTOM:
		ret = cion_custom(c, d.custom.cmd, u64_to_user_ptr(d.custom.arg), false);
		break;
	case ION_IOC_CUSTOM32:
		ret = cion_custom(c, d.custom32.cmd, compat_ptr(d.custom32.arg), true);
		break;
	case ION_IOC_HEAP_QUERY:
		ret = cion_heap_query(&d.query);
		break;
	default:
		ret = -ENOTTY;
		break;
	}
	mutex_unlock(&c->lock);

	if (ret == 0 && (_IOC_DIR(cmd) & _IOC_READ) &&
	    copy_to_user(uarg, &d, _IOC_SIZE(cmd)))
		return -EFAULT;
	return ret;
}

static long cion_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return cion_ioctl_common(file, cmd, arg, false);
}

#ifdef CONFIG_COMPAT
static long cion_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return cion_ioctl_common(file, cmd, (unsigned long)compat_ptr(arg), true);
}
#endif

static int cion_open(struct inode *inode, struct file *file)
{
	struct cion_client *c = kzalloc(sizeof(*c), GFP_KERNEL);

	if (!c)
		return -ENOMEM;
	mutex_init(&c->lock);
	idr_init(&c->handles);
	strscpy(c->name, current->comm, sizeof(c->name));
	file->private_data = c;
	return 0;
}

static int cion_release_handle(int id, void *p, void *data)
{
	cion_handle_destroy(p);
	return 0;
}

static int cion_release(struct inode *inode, struct file *file)
{
	struct cion_client *c = file->private_data;

	idr_for_each(&c->handles, cion_release_handle, NULL);
	idr_destroy(&c->handles);
	kfree(c);
	return 0;
}

static const struct file_operations cion_fops = {
	.owner = THIS_MODULE,
	.open = cion_open,
	.release = cion_release,
	.unlocked_ioctl = cion_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cion_compat_ioctl,
#endif
};

static struct miscdevice cion_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ion",
	.fops = &cion_fops,
};

static int __init cion_init(void)
{
	return misc_register(&cion_misc);
}
module_init(cion_init);

static void __exit cion_exit(void)
{
	int i;

	misc_deregister(&cion_misc);
	for (i = 0; i < CION_NR_DOM; i++)
		if (cion_dom_dev[i])
			put_device(cion_dom_dev[i]);
}
module_exit(cion_exit);

MODULE_DESCRIPTION("camellia: 4.14 MediaTek ION (legacy and modern) on 6.6 dma-heaps for the stock vendor");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
