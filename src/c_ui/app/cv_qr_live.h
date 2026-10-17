#ifndef QRS_CV_QR_LIVE_H
#define QRS_CV_QR_LIVE_H

/*
 * 扫压生命周期 → 二维码载荷的胶水层。
 *
 * 只在 UI 主循环上下文调用(与 `UI_Demo_*` 同一线程)。
 * 三路都完整才生成;任何一步失败都会清空传输页并打 `[qr]` 日志。
 */

#ifdef __cplusplus
extern "C" {
#endif

/* 扫压开始 / 复位:清掉上一次的二维码,传输页回到 DEMO 状态。 */
void CV_QR_Live_Reset(void);

/* 扫压结束(完成或停止):复制三路数据 → 生成 URL → 推给 UI。 */
void CV_QR_Live_OnSweepComplete(void);

#ifdef __cplusplus
}
#endif

#endif
