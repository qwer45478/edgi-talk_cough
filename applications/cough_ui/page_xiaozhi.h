/*
 * page_xiaozhi.h — 小智AI助手页面
 *
 * 功能：封装官方SDK的xiaozhi_ui，集成到cough_ui框架
 *
 * @yyc add
 */

#ifndef PAGE_XIAOZHI_H
#define PAGE_XIAOZHI_H

#include <rtthread.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 页面管理器API ===== */

/**
 * @brief 初始化小智助手页面
 * @note 在系统启动时调用，确保xiaozhi_ui资源就绪
 */
void page_xiaozhi_init(void);

/**
 * @brief 创建小智助手页面
 * @param parent 父对象（通常是一个全屏容器）
 *
 * 调用此函数会创建完整的小智助手UI，包括：
 * - 状态显示区域
 * - 对话输出区域
 * - 表情显示
 * - 提示文字
 */
void page_xiaozhi_create(lv_obj_t *parent);

/**
 * @brief 销毁小智助手页面
 * @note 退出小智模式时调用，释放UI资源
 */
void page_xiaozhi_destroy(void);

/**
 * @brief 更新小智助手状态显示
 * @param status 状态文本（如"待命中"、"聆听中..."、"思考中..."）
 */
void page_xiaozhi_set_status(const char *status);

/**
 * @brief 更新小智助手输出文本
 * @param output 要显示的输出文本（如识别到的语音、AI回复）
 */
void page_xiaozhi_set_output(const char *output);

/**
 * @brief 清空输出文本
 */
void page_xiaozhi_clear_output(void);

/**
 * @brief 更新emoji表情
 * @param emoji 表情名称（如"happy", "sad", "neutral", "thinking", "speaking"）
 */
void page_xiaozhi_set_emoji(const char *emoji);

/**
 * @brief 显示AP配网信息
 * @param ap_info AP信息字符串
 */
void page_xiaozhi_show_ap_config(const char *ap_info);

/**
 * @brief 显示连接中状态
 */
void page_xiaozhi_show_connecting(void);

/**
 * @brief 更新电池电量
 * @param level 电量百分比(0-100)
 */
void page_xiaozhi_update_battery(int level);

/**
 * @brief 更新充电状态
 * @param is_charging 是否正在充电
 */
void page_xiaozhi_update_charging_status(bool is_charging);

#ifdef __cplusplus
}
#endif

#endif /* PAGE_XIAOZHI_H */
