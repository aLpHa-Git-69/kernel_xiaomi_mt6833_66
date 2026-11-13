// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "inc/tcpm.h"
#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "pd_dpm_prv.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_pdo_select.h"
#endif	/* CONFIG_USB_POWER_DELIVERY */


/* Check status */
static int tcpm_check_typec_attached(struct tcpc_device *tcpc)
{
	if (tcpc->typec_attach_old == TYPEC_UNATTACHED ||
		tcpc->typec_attach_new == TYPEC_UNATTACHED)
		return TCPM_ERROR_UNATTACHED;

	return TCPM_SUCCESS;
}


#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
int tcpm_check_pd_attached(struct tcpc_device *tcpc)
{
	int ret = TCPM_SUCCESS;
	struct pd_port *pd_port = &tcpc->pd_port;
	struct pe_data *pe_data = &pd_port->pe_data;

	tcpci_lock_typec(tcpc);

	if (!tcpc->pd_inited_flag) {
		ret = TCPM_ERROR_PE_NOT_READY;
		goto unlock_typec_out;
	}

	ret = tcpm_check_typec_attached(tcpc);

unlock_typec_out:
	tcpci_unlock_typec(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	mutex_lock(&pd_port->pd_lock);

	if (!pe_data->pd_prev_connected) {
		ret = TCPM_ERROR_NO_PD_CONNECTED;
		goto unlock_pd_out;
	}

	if (!pe_data->pe_ready)
		ret = TCPM_ERROR_PE_NOT_READY;

unlock_pd_out:
	mutex_unlock(&pd_port->pd_lock);
	return ret;
}
EXPORT_SYMBOL(tcpm_check_pd_attached);
#endif	/* CONFIG_USB_POWER_DELIVERY */


/* Inquire TCPC status */

int tcpm_shutdown(struct tcpc_device *tcpc)
{
	tcpci_lock_typec(tcpc);
#if CONFIG_TCPC_SHUTDOWN_VBUS_DISABLE
	if (tcpc->typec_power_ctrl)
		tcpci_disable_vbus_control(tcpc);
#endif	/* CONFIG_TCPC_SHUTDOWN_VBUS_DISABLE */

	if (tcpc->ops->deinit)
		tcpc->ops->deinit(tcpc);
	tcpc->pd_inited_flag = 0;
	tcpci_unlock_typec(tcpc);

	return 0;
}
EXPORT_SYMBOL(tcpm_shutdown);

int tcpm_suspend(struct tcpc_device *tcpc)
{
	int ret = 0;

	/*
	 * the following 3 if statements are lockless solutions
	 * for preventing some race conditions
	 */
	if (atomic_read(&tcpc->suspend_pending) > 0)
		return -EBUSY;

	if (atomic_read(&tcpc->pending_event) > 0 ||
	    tcpc_get_timer_tick(tcpc))
		return -EBUSY;

	if (atomic_read(&tcpc->suspend_pending) > 0)
		return -EBUSY;

#if CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND
	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_suspend(tcpc);
	tcpci_unlock_typec(tcpc);
#endif	/* CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND */

	return ret;
}
EXPORT_SYMBOL(tcpm_suspend);

void tcpm_resume(struct tcpc_device *tcpc)
{
#if CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND
	tcpci_lock_typec(tcpc);
	tcpc_typec_resume(tcpc);
	tcpci_unlock_typec(tcpc);
#endif	/* CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND */
}
EXPORT_SYMBOL(tcpm_resume);

int tcpm_inquire_remote_cc(struct tcpc_device *tcpc,
	uint8_t *cc1, uint8_t *cc2, bool from_ic)
{
	int rv = 0;

	tcpci_lock_typec(tcpc);
	if (from_ic) {
		rv = tcpci_get_cc(tcpc);
		if (rv < 0)
			goto out;
	}

	*cc1 = tcpc->typec_remote_cc[0];
	*cc2 = tcpc->typec_remote_cc[1];
out:
	tcpci_unlock_typec(tcpc);
	return rv;
}
EXPORT_SYMBOL(tcpm_inquire_remote_cc);

int tcpm_inquire_typec_remote_rp_curr(struct tcpc_device *tcpc)
{
	if (tcpm_check_typec_attached(tcpc))
		return 0;

	switch (tcpc->typec_remote_rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return 1500;
	case TYPEC_CC_VOLT_SNK_3_0:
		return 3000;
	case TYPEC_CC_VOLT_SNK_DFT:
	default:
		return 500;
	}
}
EXPORT_SYMBOL(tcpm_inquire_typec_remote_rp_curr);

int tcpm_inquire_vbus_level(struct tcpc_device *tcpc, bool from_ic)
{
	int rv = 0;

	tcpci_lock_typec(tcpc);
	if (from_ic) {
		rv = tcpci_get_power_status(tcpc);
		if (rv < 0)
			return rv;
	}
	rv = tcpc->vbus_level;
	tcpci_unlock_typec(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpm_inquire_vbus_level);

int tcpm_inquire_cc_high(struct tcpc_device *tcpc)
{
	int rv = TCPM_ERROR_UNKNOWN;

	tcpci_lock_typec(tcpc);
	if (!tcpc->cc_hidet_en)
		goto out;
	if (tcpc->ops->get_cc_hi)
		rv = tcpc->ops->get_cc_hi(tcpc);
out:
	tcpci_unlock_typec(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpm_inquire_cc_high);

uint8_t tcpm_inquire_typec_attach_state(struct tcpc_device *tcpc)
{
	return tcpc->typec_attach_new;
}
EXPORT_SYMBOL(tcpm_inquire_typec_attach_state);

uint8_t tcpm_inquire_typec_role_def(struct tcpc_device *tcpc)
{
	return tcpc->desc.role_def;
}
EXPORT_SYMBOL(tcpm_inquire_typec_role_def);

int tcpm_typec_role_swap(struct tcpc_device *tcpc)
{
#if CONFIG_TYPEC_CAP_ROLE_SWAP
	int ret = tcpm_check_typec_attached(tcpc);

	if (ret != TCPM_SUCCESS)
		return ret;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_swap_role(tcpc);
	tcpci_unlock_typec(tcpc);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif /* CONFIG_TYPEC_CAP_ROLE_SWAP */
}
EXPORT_SYMBOL(tcpm_typec_role_swap);

int tcpm_typec_change_role(
	struct tcpc_device *tcpc, uint8_t typec_role)
{
	int ret = 0;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_change_role(tcpc, typec_role, false);
	tcpci_unlock_typec(tcpc);

	return ret;
}
EXPORT_SYMBOL(tcpm_typec_change_role);

/* @postpone: whether to postpone Type-C role change until unattached */
int tcpm_typec_change_role_postpone(
	struct tcpc_device *tcpc, uint8_t typec_role, bool postpone)
{
	int ret = 0;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_change_role(tcpc, typec_role, postpone);
	tcpci_unlock_typec(tcpc);

	return ret;
}
EXPORT_SYMBOL(tcpm_typec_change_role_postpone);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)

uint8_t tcpm_inquire_pd_data_role(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->data_role;
}
EXPORT_SYMBOL(tcpm_inquire_pd_data_role);

uint8_t tcpm_inquire_pd_power_role(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->power_role;
}
EXPORT_SYMBOL(tcpm_inquire_pd_power_role);

uint8_t tcpm_inquire_pd_vconn_role(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;
	uint8_t vconn_role = pd_port->vconn_role;
#if CONFIG_USB_PD_VCONN_SAFE5V_ONLY
	struct pe_data *pe_data = &pd_port->pe_data;

	if (pe_data->vconn_highv_prot)
		vconn_role = pe_data->vconn_highv_prot_role;
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */

	return vconn_role;
}
EXPORT_SYMBOL(tcpm_inquire_pd_vconn_role);

uint8_t tcpm_inquire_pd_pe_ready(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->pe_data.pe_ready;
}
EXPORT_SYMBOL(tcpm_inquire_pd_pe_ready);

uint32_t tcpm_inquire_dpm_flags(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->pe_data.dpm_flags;
}
EXPORT_SYMBOL(tcpm_inquire_dpm_flags);

/* Inquire TCPC to get PD Information */

int tcpm_inquire_pd_partner_inform(struct tcpc_device *tcpc, uint32_t *vdos)
{
#if CONFIG_USB_PD_KEEP_PARTNER_ID
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (vdos == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.partner_id_present) {
		memcpy(vdos, pd_port->pe_data.partner_vdos,
			sizeof(uint32_t) * VDO_MAX_NR);
	} else
		ret = TCPM_ERROR_NO_PARTNER_INFORM;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif	/* CONFIG_USB_PD_KEEP_PARTNER_ID */
}
EXPORT_SYMBOL(tcpm_inquire_pd_partner_inform);

int tcpm_inquire_pd_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (cap == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.remote_src_cap.nr) {
		cap->cnt = pd_port->pe_data.remote_src_cap.nr;
		memcpy(cap->pdos, pd_port->pe_data.remote_src_cap.pdos,
			sizeof(uint32_t) * cap->cnt);
	} else
		ret = TCPM_ERROR_NO_SOURCE_CAP;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_pd_source_cap);

bool tcpm_extract_power_cap_val(uint32_t pdo, struct tcpm_power_cap_val *cap)
{
	struct dpm_pdo_info_t info;

	dpm_extract_pdo_info(pdo, &info);

	cap->type = info.type;
	cap->min_mv = info.vmin;
	cap->max_mv = info.vmax;

	if (info.type == DPM_PDO_TYPE_BAT)
		cap->uw = info.uw;
	else
		cap->ma = info.ma;

#if CONFIG_USB_PD_REV30
	if (info.type == DPM_PDO_TYPE_APDO) {
		cap->apdo_type = info.apdo_type;
		cap->pwr_limit = info.pwr_limit;
	}
#endif	/* CONFIG_USB_PD_REV30 */

	return cap->type != TCPM_POWER_CAP_VAL_TYPE_UNKNOWN;
}
EXPORT_SYMBOL(tcpm_extract_power_cap_val);

int tcpm_get_remote_power_cap(struct tcpc_device *tcpc,
		struct tcpm_remote_power_cap *remote_cap)
{
	struct pd_port *pd_port = &tcpc->pd_port;
	struct tcpm_power_cap_val cap;
	int i;

	mutex_lock(&pd_port->pd_lock);
	remote_cap->selected_cap_idx = pd_port->pe_data.selected_cap;
	remote_cap->nr = pd_port->pe_data.remote_src_cap.nr;
	for (i = 0; i < remote_cap->nr; i++) {
		tcpm_extract_power_cap_val(
			pd_port->pe_data.remote_src_cap.pdos[i], &cap);
		remote_cap->max_mv[i] = cap.max_mv;
		remote_cap->min_mv[i] = cap.min_mv;
		if (cap.type == TCPM_POWER_CAP_VAL_TYPE_BATTERY)
			remote_cap->ma[i] = cap.uw / cap.min_mv;
		else
			remote_cap->ma[i] = cap.ma;
		remote_cap->type[i] = cap.type;
	}
	mutex_unlock(&pd_port->pd_lock);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_get_remote_power_cap);

static inline int __tcpm_inquire_select_source_cap(
	struct pd_port *pd_port, struct tcpm_power_cap_val *cap_val)
{
	uint8_t sel;
	struct pe_data *pe_data = &pd_port->pe_data;

	if (pd_port->power_role != PD_ROLE_SINK)
		return TCPM_ERROR_POWER_ROLE;

	sel = RDO_POS(pd_port->last_rdo) - 1;
	if (sel >= pe_data->remote_src_cap.nr)
		return TCPM_ERROR_NO_SOURCE_CAP;

	if (!tcpm_extract_power_cap_val(
		pe_data->remote_src_cap.pdos[sel], cap_val))
		return TCPM_ERROR_NOT_FOUND;

	return TCPM_SUCCESS;
}

bool tcpm_inquire_usb_comm(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return !!(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_USB_COMM);
}
EXPORT_SYMBOL(tcpm_inquire_usb_comm);
/* Request TCPC to send PD Request */

#if CONFIG_USB_PD_BLOCK_TCPM

#define TCPM_BK_PD_CMD_TOUT	500

/* tPSTransition 550 ms */
#define TCPM_BK_REQUEST_TOUT		1500

/* tPSSourceOff 920 ms,	tPSSourceOn 480 ms*/
#define TCPM_BK_PR_SWAP_TOUT		2500
#define TCPM_BK_HARD_RESET_TOUT	3500

static int tcpm_put_tcp_dpm_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event);

static int tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size);
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

static inline int tcpm_put_tcp_dpm_event_cb(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	if (cb_data) {
		event->user_data = cb_data->user_data;
		event->event_cb = cb_data->event_cb;
	}
	return tcpm_put_tcp_dpm_event(tcpc, event);
}

static int tcpm_put_tcp_dpm_event_cbk1(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data, uint32_t tout_ms)
{
#if CONFIG_USB_PD_BLOCK_TCPM
	if (cb_data == NULL) {
		return tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, NULL, 0);
	}
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

	return tcpm_put_tcp_dpm_event_cb(tcpc, event, cb_data);
}

int tcpm_dpm_pd_power_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_PR_SWAP_AS_SNK + role,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PR_SWAP_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_power_swap);

int tcpm_dpm_pd_data_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DR_SWAP_AS_UFP + role,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_data_swap);

int tcpm_dpm_pd_vconn_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_VCONN_SWAP_OFF + role,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_vconn_swap);

int tcpm_dpm_pd_soft_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_SOFTRESET,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_soft_reset);

int tcpm_dpm_pd_get_source_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SOURCE_CAP,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_source_cap);

int tcpm_dpm_pd_get_sink_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SINK_CAP,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_sink_cap);

int tcpm_dpm_pd_request(struct tcpc_device *tcpc,
	int mv, int ma, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST,
		.tcp_dpm_data.pd_req.mv = mv,
		.tcp_dpm_data.pd_req.ma = ma,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_request);

#if CONFIG_USB_PD_REV30

static int tcpm_put_tcp_dpm_event_cbk2(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
#if CONFIG_USB_PD_BLOCK_TCPM
	if (cb_data == NULL) {
		return tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, data, size);
	}
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

	return tcpm_put_tcp_dpm_event_cb(tcpc, event, cb_data);
}

int tcpm_dpm_pd_get_source_cap_ext(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_source_cap_ext *src_cap_ext)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SOURCE_CAP_EXT,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) src_cap_ext, PD_SCEDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_source_cap_ext);

int tcpm_dpm_pd_get_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data, struct pd_status *status)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_STATUS,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) status, PD_SDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_status);

int tcpm_dpm_pd_get_pps_status_raw(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_pps_status_raw *pps_status)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_PPS_STATUS,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) pps_status, PD_PPSSDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_pps_status_raw);

int tcpm_dpm_pd_get_pps_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_pps_status *pps_status)
{
	int ret;
	struct pd_pps_status_raw pps_status_raw;

	ret = tcpm_dpm_pd_get_pps_status_raw(
		tcpc, cb_data, &pps_status_raw);

	if (ret != 0)
		return ret;

	if (pps_status_raw.output_vol_raw == 0xffff)
		pps_status->output_mv = -1;
	else
		pps_status->output_mv =
			PD_PPS_GET_OUTPUT_MV(pps_status_raw.output_vol_raw);

	if (pps_status_raw.output_curr_raw == 0xff)
		pps_status->output_ma = -1;
	else
		pps_status->output_ma =
			PD_PPS_GET_OUTPUT_MA(pps_status_raw.output_curr_raw);

	pps_status->real_time_flags = pps_status_raw.real_time_flags;
	return ret;
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_pps_status);

#endif	/* CONFIG_USB_PD_REV30 */

int tcpm_dpm_pd_hard_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_HARD_RESET,
	};

	/* check not block case !!! */
	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_HARD_RESET_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_hard_reset);

int tcpm_dpm_vdm_discover_cable_id(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_CABLE_ID,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_discover_cable_id);

int tcpm_dpm_vdm_discover_id(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_ID,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_discover_id);

#if CONFIG_USB_PD_TCPM_CB_2ND
static void tcpm_replace_curr_tcp_event(
	struct pd_port *pd_port, struct tcp_dpm_event *event)
{
	int reason = TCP_DPM_RET_DENIED_UNKNOWN;

	switch (event->event_id) {
	case TCP_DPM_EVT_HARD_RESET:
		reason = TCP_DPM_RET_DROP_SENT_HRESET;
		break;
	case TCP_DPM_EVT_ERROR_RECOVERY:
		reason = TCP_DPM_RET_DROP_ERROR_REOCVERY;
		break;
	}

	mutex_lock(&pd_port->pd_lock);
	pd_notify_tcp_event_2nd_result(pd_port, reason);
	pd_port->tcp_event_drop_reset_once = true;
	pd_port->tcp_event_id_2nd = event->event_id;
	memcpy(&pd_port->tcp_event, event, sizeof(struct tcp_dpm_event));
	mutex_unlock(&pd_port->pd_lock);
}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

static int tcpm_put_tcp_dpm_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event)
{
	int ret;
	bool imme = event->event_id >= TCP_DPM_EVT_IMMEDIATELY;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS &&
		(imme && ret == TCPM_ERROR_UNATTACHED))
		return ret;

	if (imme) {
		ret = pd_put_tcp_pd_event(pd_port, event->event_id,
					  PD_TCP_FROM_TCPM);

#if CONFIG_USB_PD_TCPM_CB_2ND
		if (ret)
			tcpm_replace_curr_tcp_event(pd_port, event);
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
	} else {
		mutex_lock(&pd_port->pd_lock);
		ret = pd_put_deferred_tcp_event(tcpc, event);
		mutex_unlock(&pd_port->pd_lock);
	}

	if (!ret)
		return TCPM_ERROR_PUT_EVENT;

	return TCPM_SUCCESS;
}

int tcpm_reset_pd_charging_policy(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	return tcpm_set_pd_charging_policy(
		tcpc, tcpc->pd_port.dpm_charging_policy_default, cb_data);
}
EXPORT_SYMBOL(tcpm_reset_pd_charging_policy);

int tcpm_set_pd_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_AGAIN,
	};

	struct pd_port *pd_port = &tcpc->pd_port;

	/* PPS should call another function ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->dpm_charging_policy == policy) {
		mutex_unlock(&pd_port->pd_lock);
		return TCPM_SUCCESS;
	}

	pd_port->dpm_charging_policy = policy;
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_set_pd_charging_policy);

#if CONFIG_USB_PD_DIRECT_CHARGE

#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

static int tcpm_put_tcp_dummy_event(struct tcpc_device *tcpc)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DUMMY,
	};

	return tcpm_put_tcp_dpm_event(tcpc, &tcp_event);
}

#if CONFIG_USB_PD_REV30_PPS_SINK

int tcpm_set_apdo_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, int mv, int ma,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_AGAIN,
	};

	struct pd_port *pd_port = &tcpc->pd_port;

	/* Not PPS should call another function ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) < DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (!pd_is_source_support_apdo(pd_port)) {
		mutex_unlock(&pd_port->pd_lock);
		return TCPM_ERROR_INVALID_POLICY;
	}

	if (pd_port->dpm_charging_policy == policy) {
		mutex_unlock(&pd_port->pd_lock);
		return tcpm_dpm_pd_request(tcpc, mv, ma, NULL);
	}

	pd_port->dpm_charging_policy = policy;
	pd_port->request_v_apdo = mv;
	pd_port->request_i_apdo = ma;
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_set_apdo_charging_policy);

int tcpm_inquire_pd_source_apdo(struct tcpc_device *tcpc,
	uint8_t apdo_type, uint8_t *cap_i, struct tcpm_power_cap_val *cap_val)
{
	int ret;
	uint8_t i;
	struct tcpm_power_cap cap;

	ret = tcpm_inquire_pd_source_cap(tcpc, &cap);
	if (ret != TCPM_SUCCESS)
		return ret;

	for (i = *cap_i; i < cap.cnt; i++) {
		if (!tcpm_extract_power_cap_val(cap.pdos[i], cap_val))
			continue;

		if (cap_val->type != TCPM_POWER_CAP_VAL_TYPE_AUGMENT)
			continue;

		if ((cap_val->apdo_type & TCPM_APDO_TYPE_MASK) != apdo_type)
			continue;

		*cap_i = i+1;
		return TCPM_SUCCESS;
	}

	return TCPM_ERROR_NOT_FOUND;
}
EXPORT_SYMBOL(tcpm_inquire_pd_source_apdo);

#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#if CONFIG_USB_PD_REV30_BAT_INFO

static void tcpm_alert_bat_changed(
	struct pd_port *pd_port, enum pd_battery_reference ref)
{
#if CONFIG_USB_PD_REV30_ALERT_LOCAL
	uint8_t fixed = 0, swap = 0;

	if (ref >= PD_BAT_REF_SWAP0)
		swap = 1 << (ref - PD_BAT_REF_SWAP0);
	else
		fixed = 1 << (ref - PD_BAT_REF_FIXED0);

	pd_port->pe_data.local_alert |=
		ADO(ADO_ALERT_BAT_CHANGED, fixed, swap);
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
}

static inline uint16_t tcpm_calc_battery_cap(
	struct pd_battery_info *battery_info, uint16_t soc)
{
	uint16_t wh;
	struct pd_battery_capabilities *bat_cap;

	bat_cap = &battery_info->bat_cap;

	if (bat_cap->bat_last_full_cap != BSDO_BAT_CAP_UNKNOWN)
		wh = bat_cap->bat_last_full_cap;
	else if (bat_cap->bat_design_cap != BSDO_BAT_CAP_UNKNOWN)
		wh = bat_cap->bat_design_cap;
	else
		wh = BSDO_BAT_CAP_UNKNOWN;

	if (wh != BSDO_BAT_CAP_UNKNOWN)
		wh = wh * soc / 1000;

	return wh;
}

static int tcpm_update_bsdo(
	struct pd_port *pd_port, enum pd_battery_reference ref,
	uint16_t soc, uint16_t wh, uint8_t status)
{
	uint8_t status_old;
	struct pd_battery_info *battery_info;

	battery_info = pd_get_battery_info(pd_port, ref);
	if (battery_info == NULL)
		return TCPM_ERROR_PARAMETER;

	status_old = BSDO_BAT_INFO(battery_info->bat_status);

	if (soc != 0)
		wh = tcpm_calc_battery_cap(battery_info, soc);

	battery_info->bat_status = BSDO(wh, status);

	if (status_old != status) {
		tcpm_alert_bat_changed(pd_port, ref);
		return TCPM_ALERT;
	}

	return TCPM_SUCCESS;
}

static inline int tcpm_update_bat_status_wh_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh)
{
	return tcpm_update_bsdo(&tcpc->pd_port, ref, 0, wh, status);
}

static inline int tcpm_update_bat_status_soc_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t soc)
{
	return tcpm_update_bsdo(&tcpc->pd_port, ref, soc, 0, status);
}

int tcpm_update_bat_status_soc(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t soc)
{
	int ret;

	mutex_lock(&tcpc->pd_port.pd_lock);
	ret = tcpm_update_bat_status_soc_no_mutex(tcpc, ref, status, soc);
	mutex_unlock(&tcpc->pd_port.pd_lock);

	if (ret == TCPM_ALERT) {
		ret = TCPM_SUCCESS;
		tcpm_put_tcp_dummy_event(tcpc);
	}

	return ret;
}
EXPORT_SYMBOL(tcpm_update_bat_status_soc);

static inline int tcpm_update_bat_last_full_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh)
{
	struct pd_battery_info *battery_info;

	battery_info = pd_get_battery_info(&tcpc->pd_port, ref);
	if (battery_info == NULL)
		return TCPM_ERROR_PARAMETER;

	battery_info->bat_cap.bat_last_full_cap = wh;
	return TCPM_SUCCESS;
}

#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */

#if CONFIG_USB_PD_REV30_STATUS_LOCAL

#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */

#if CONFIG_USB_PD_BLOCK_TCPM

#if TCPM_DBG_ENABLE
static const char * const bk_event_ret_name[] = {
	"OK",
	"Unknown",	/* or not support by TCPM */
	"NotReady",
	"LocalCap",
	"PartnerCap",
	"SameRole",
	"InvalidReq",
	"RepeatReq",
	"WrongR",
	"PDRev",
	"ModalOperation",
#if CONFIG_USB_PD_VCONN_SAFE5V_ONLY
	"VconnHighVProt",
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */

	"Detach",
	"SReset0",
	"SReset1",
	"HReset0",
	"HReset1",
	"Recovery",
	"BIST",
	"PEBusy",
	"Discard",
	"Unexpected",

	"Wait",
	"Reject",
	"TOUT",
	"NAK",
	"NotSupport",

	"BKTOUT",
	"NoResponse",
};
#endif /* TCPM_DBG_ENABLE */

#if CONFIG_USB_PD_TCPM_CB_2ND
static inline void tcpm_dpm_bk_copy_data(struct pd_port *pd_port)
{
	uint8_t size = pd_port->tcpm_bk_cb_data_max;

	if (size >= pd_get_msg_data_size(pd_port))
		size = pd_get_msg_data_size(pd_port);

	if (pd_port->tcpm_bk_cb_data != NULL) {
		memcpy(pd_port->tcpm_bk_cb_data,
			pd_get_msg_data_payload(pd_port), size);
	}
}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

int tcpm_dpm_bk_event_cb(
	struct tcpc_device *tcpc, int ret, struct tcp_dpm_event *event)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	if (pd_port->tcpm_bk_event_id != event->event_id) {
		TCPM_DBG("bk_event_cb_dummy: expect:%d real:%d\n",
			pd_port->tcpm_bk_event_id, event->event_id);
		return 0;
	}

	pd_port->tcpm_bk_ret = ret;
	pd_port->tcpm_bk_done = true;

#if CONFIG_USB_PD_TCPM_CB_2ND
	if (ret == TCP_DPM_RET_SUCCESS)
		tcpm_dpm_bk_copy_data(pd_port);
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	wake_up(&pd_port->tcpm_bk_wait_que);
	return 0;
}
EXPORT_SYMBOL(tcpm_dpm_bk_event_cb);

static inline int __tcpm_dpm_wait_bk_event(
	struct pd_port *pd_port, uint32_t tout_ms)
{
	int ret = TCP_DPM_RET_BK_TIMEOUT;

	wait_event_timeout(pd_port->tcpm_bk_wait_que, pd_port->tcpm_bk_done,
			   msecs_to_jiffies(tout_ms));

	if (pd_port->tcpm_bk_done)
		return pd_port->tcpm_bk_ret;
	mutex_lock(&pd_port->pd_lock);

	pd_port->tcpm_bk_event_id = TCP_DPM_EVT_UNKNOWN;

#if CONFIG_USB_PD_TCPM_CB_2ND
	pd_port->tcpm_bk_cb_data = NULL;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

static inline int tcpm_dpm_wait_bk_event(
	struct pd_port *pd_port, uint32_t tout_ms)
{
	int ret = __tcpm_dpm_wait_bk_event(pd_port, tout_ms);
#if TCPM_DBG_ENABLE
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ret < TCP_DPM_RET_NR && ret >= 0)
		TCPM_DBG("bk_event_cb -> %s\n", bk_event_ret_name[ret]);
#endif /* TCPM_DBG_ENABLE */

	return ret;
}

#if CONFIG_MTK_HANDLE_PPS_TIMEOUT
static void mtk_handle_tcp_event_result(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event, int ret)
{
	struct tcp_dpm_event evt_hreset = {
		.event_id = TCP_DPM_EVT_HARD_RESET,
	};

	if (ret == TCPM_SUCCESS || ret == TCP_DPM_RET_NOT_SUPPORT)
		return;

	if (event->event_id == TCP_DPM_EVT_GET_STATUS
		|| event->event_id == TCP_DPM_EVT_GET_PPS_STATUS) {
		tcpm_put_tcp_dpm_event(tcpc, &evt_hreset);
	}
}
#endif /* CONFIG_MTK_HANDLE_PPS_TIMEOUT */

static int __tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	pd_port->tcpm_bk_done = false;
	pd_port->tcpm_bk_event_id = event->event_id;

#if CONFIG_USB_PD_TCPM_CB_2ND
	pd_port->tcpm_bk_cb_data = data;
	pd_port->tcpm_bk_cb_data_max = size;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	ret = tcpm_put_tcp_dpm_event(tcpc, event);
	if (ret != TCPM_SUCCESS)
		return ret;

	return tcpm_dpm_wait_bk_event(pd_port, tout_ms);
}

static int tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
	int ret = TCP_DPM_RET_TIMEOUT, retry = CONFIG_USB_PD_TCPM_CB_RETRY;
	struct pd_port *pd_port = &tcpc->pd_port;

	event->event_cb = tcpm_dpm_bk_event_cb;

	mutex_lock(&pd_port->tcpm_bk_lock);

	do {
		ret = __tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, data, size);
	} while ((ret == TCP_DPM_RET_TIMEOUT ||
		  ret == TCP_DPM_RET_DROP_DISCARD ||
		  ret == TCP_DPM_RET_DROP_UNEXPECTED) && (retry-- > 0));

	mutex_unlock(&pd_port->tcpm_bk_lock);

#if !CONFIG_USB_PD_TCPM_CB_2ND
	if ((data != NULL) && (ret == TCPM_SUCCESS))
		return TCPM_ERROR_EXPECT_CB2;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	if (ret == TCP_DPM_RET_DENIED_REPEAT_REQUEST)
		ret = TCPM_SUCCESS;

#if CONFIG_MTK_HANDLE_PPS_TIMEOUT
	mtk_handle_tcp_event_result(tcpc, event, ret);
#endif /* CONFIG_MTK_HANDLE_PPS_TIMEOUT */

	return ret;
}

#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

#endif /* CONFIG_USB_POWER_DELIVERY */
