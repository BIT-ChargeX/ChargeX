#pragma once
#include <QString>

// 阿里云短信服务接入（SendSms，RPC 签名 v1.0）
// 使用前请先完成：阿里云控制台 -> 短信服务 -> 开通、申请签名、申请模板。
// 模板内容需形如「您的验证码为 ${code}，5 分钟内有效」，否则下方 TemplateParam 的 code 变量无效。
// 注意：本文件包含密钥，请勿提交到公开仓库。
namespace AliyunSms {

// ---- 请替换为你的真实凭证 ----
inline constexpr const char* kAccessKeyId     = "";   // AccessKey ID（控制台 RAM 用户）
inline constexpr const char* kAccessKeySecret = "";   // AccessKey Secret
inline constexpr const char* kSignName        = "";   // 短信签名（审核通过后的签名名称）
inline constexpr const char* kTemplateCode    = "";   // 模板 CODE，如 SMS_123456

// 同步发送一条登录验证码短信；成功返回 true，失败把原因写入 err（可为空）。
bool send(const QString& phone, const QString& code, QString* err = nullptr);

}
