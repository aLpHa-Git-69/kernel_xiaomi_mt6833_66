// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2021 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "protocol_common.h"
#include "protocol.h"
#include "main.h"
#include "mmu.h"
#include "client.h"

/* Call Frontend */

static inline void protocol_be_get(struct protocol_fe *pfe)
{
	protocol_get(pfe);
	memset(pfe->be2fe_data, 0, sizeof(*pfe->be2fe_data));
}

/* MC protocol interface */

/* mc_wait cannot keep the channel busy while waiting, so we use a worker */
struct mc_wait_work {
	struct work_struct	work;
	struct protocol_fe	*pfe;
	u32			session_id;
	s32			timeout;
	u32			id;
	atomic_t		vm_instance_no;
};

/* GP protocol interface */

/* GP functions cannot keep the ring busy while waiting, so we use a worker */
struct gp_work {
	struct work_struct		work;
	struct protocol_fe		*pfe;
	u64				operation_id;
	struct interworld_session	iws;
	struct tee_mmu			*ta_mmu;
	struct tee_mmu			*mmus[4];
	struct mc_uuid_t		uuid;
	u32				session_id;
	u32				id;
	atomic_t			vm_instance_no;
};

/* Once Backend/Frontend connection established and VMs data struct to control
 * comm and exchange messages (struct tee_xfe) configured successfully,
 * TEE Commands managed here.
 */

