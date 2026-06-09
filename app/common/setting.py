# coding: utf-8
import sys
from pathlib import Path

from PySide6.QtCore import QCoreApplication

AUTHOR = "baby2016"
TEAM = "天机阁(Fairy-Oracle-Sanctuary)"
VERSION = "2.0.0"
YEAR = "2025"
UPDATE_TIME = "2026-6-30"
if sys.platform == "win32":
    COPYLEFT = "🄯 "
else:
    COPYLEFT = "©️ "

RELEASE_URL = "https://github.com/Fairy-Oracle-Sanctuary/Touhou-translate/releases"
GITHUB_URL = "https://github.com/Fairy-Oracle-Sanctuary/Touhou-translate"

CONFIG_FOLDER = Path("AppData").absolute()

CONFIG_FILE = CONFIG_FOLDER / "config.json"
DB_PATH = CONFIG_FOLDER / "database.db"

COVER_FOLDER = CONFIG_FOLDER / "Cover"
COVER_FOLDER.mkdir(exist_ok=True, parents=True)

PIC_SUFFIX = ".jpg"

# videocr
videocr_languages_dict = {
    "ch": QCoreApplication.translate("Language", "中文与英文"),
    "chinese_cht": QCoreApplication.translate("Language", "繁体中文"),
    "en": QCoreApplication.translate("Language", "英语"),
    "japan": QCoreApplication.translate("Language", "日语"),
    "korean": QCoreApplication.translate("Language", "韩语"),
    "fr": QCoreApplication.translate("Language", "法语"),
    "german": QCoreApplication.translate("Language", "德语"),
    "es": QCoreApplication.translate("Language", "西班牙语"),
    "pt": QCoreApplication.translate("Language", "葡萄牙语"),
    "it": QCoreApplication.translate("Language", "意大利语"),
    "ru": QCoreApplication.translate("Language", "俄语"),
    "ar": QCoreApplication.translate("Language", "阿拉伯语"),
    "nl": QCoreApplication.translate("Language", "荷兰语"),
    "el": QCoreApplication.translate("Language", "希腊语"),
    "sv": QCoreApplication.translate("Language", "瑞典语"),
    "no": QCoreApplication.translate("Language", "挪威语"),
    "da": QCoreApplication.translate("Language", "丹麦语"),
    "fi": QCoreApplication.translate("Language", "芬兰语"),
    "pl": QCoreApplication.translate("Language", "波兰语"),
    "cs": QCoreApplication.translate("Language", "捷克语"),
    "hu": QCoreApplication.translate("Language", "匈牙利语"),
    "ro": QCoreApplication.translate("Language", "罗马尼亚语"),
    "bg": QCoreApplication.translate("Language", "保加利亚语"),
    "rs_cyrillic": QCoreApplication.translate("Language", "塞尔维亚语(西里尔文)"),
    "rs_latin": QCoreApplication.translate("Language", "塞尔维亚语(拉丁文)"),
    "hr": QCoreApplication.translate("Language", "克罗地亚语"),
    "sk": QCoreApplication.translate("Language", "斯洛伐克语"),
    "sl": QCoreApplication.translate("Language", "斯洛文尼亚语"),
    "uk": QCoreApplication.translate("Language", "乌克兰语"),
    "be": QCoreApplication.translate("Language", "白俄罗斯语"),
    "sq": QCoreApplication.translate("Language", "阿尔巴尼亚语"),
    "et": QCoreApplication.translate("Language", "爱沙尼亚语"),
    "lv": QCoreApplication.translate("Language", "拉脱维亚语"),
    "lt": QCoreApplication.translate("Language", "立陶宛语"),
    "is": QCoreApplication.translate("Language", "冰岛语"),
    "ga": QCoreApplication.translate("Language", "爱尔兰语"),
    "cy": QCoreApplication.translate("Language", "威尔士语"),
    "mt": QCoreApplication.translate("Language", "马耳他语"),
    "hi": QCoreApplication.translate("Language", "印地语"),
    "ur": QCoreApplication.translate("Language", "乌尔都语"),
    "bh": QCoreApplication.translate("Language", "孟加拉语"),
    "ta": QCoreApplication.translate("Language", "泰米尔语"),
    "te": QCoreApplication.translate("Language", "泰卢固语"),
    "mr": QCoreApplication.translate("Language", "马拉地语"),
    "th": QCoreApplication.translate("Language", "泰语"),
    "vi": QCoreApplication.translate("Language", "越南语"),
    "id": QCoreApplication.translate("Language", "印度尼西亚语"),
    "ms": QCoreApplication.translate("Language", "马来语"),
    "tl": QCoreApplication.translate("Language", "菲律宾语"),
    "fa": QCoreApplication.translate("Language", "波斯语"),
    "tr": QCoreApplication.translate("Language", "土耳其语"),
    "he": QCoreApplication.translate("Language", "希伯来语"),
    "ne": QCoreApplication.translate("Language", "尼泊尔语"),
    "si": QCoreApplication.translate("Language", "僧伽罗语"),
    "my": QCoreApplication.translate("Language", "缅甸语"),
    "km": QCoreApplication.translate("Language", "高棉语"),
    "lo": QCoreApplication.translate("Language", "老挝语"),
    "mn": QCoreApplication.translate("Language", "蒙古语"),
    "ug": QCoreApplication.translate("Language", "维吾尔语"),
    "uz": QCoreApplication.translate("Language", "乌兹别克语"),
    "sw": QCoreApplication.translate("Language", "斯瓦希里语"),
    "af": QCoreApplication.translate("Language", "南非荷兰语"),
    "la": QCoreApplication.translate("Language", "拉丁语"),
    "sa": QCoreApplication.translate("Language", "梵语"),
    "mi": QCoreApplication.translate("Language", "毛利语"),
    "abq": QCoreApplication.translate("Language", "阿巴扎语"),
    "ady": QCoreApplication.translate("Language", "阿迪格语"),
    "ang": QCoreApplication.translate("Language", "安吉卡语"),
    "ava": QCoreApplication.translate("Language", "阿瓦尔语"),
    "az": QCoreApplication.translate("Language", "阿塞拜疆语"),
    "bho": QCoreApplication.translate("Language", "博杰普尔语"),
    "bs": QCoreApplication.translate("Language", "波斯尼亚语"),
    "che": QCoreApplication.translate("Language", "车臣语"),
    "dar": QCoreApplication.translate("Language", "达尔格瓦语"),
    "gom": QCoreApplication.translate("Language", "果阿康卡尼语"),
    "bgc": QCoreApplication.translate("Language", "哈里亚纳语"),
    "inh": QCoreApplication.translate("Language", "印古什语"),
    "kbd": QCoreApplication.translate("Language", "卡巴尔达语"),
    "ku": QCoreApplication.translate("Language", "库尔德语"),
    "lbe": QCoreApplication.translate("Language", "拉克语"),
    "lez": QCoreApplication.translate("Language", "列兹金语"),
    "mah": QCoreApplication.translate("Language", "马加希语"),
    "mai": QCoreApplication.translate("Language", "迈蒂利语"),
    "sck": QCoreApplication.translate("Language", "那格浦尔语"),
    "new": QCoreApplication.translate("Language", "尼瓦尔语"),
    "oc": QCoreApplication.translate("Language", "奥克语"),
    "pi": QCoreApplication.translate("Language", "巴利语"),
    "tab": QCoreApplication.translate("Language", "塔巴萨兰语"),
    "bal": QCoreApplication.translate("Language", "俾路支语"),
    "ba": QCoreApplication.translate("Language", "巴什基尔语"),
    "eu": QCoreApplication.translate("Language", "巴斯克语"),
    "bua": QCoreApplication.translate("Language", "布里亚特语"),
    "ca": QCoreApplication.translate("Language", "加泰罗尼亚语"),
    "gl": QCoreApplication.translate("Language", "加利西亚语"),
    "ka": QCoreApplication.translate("Language", "格鲁吉亚语"),
    "xal": QCoreApplication.translate("Language", "卡尔梅克语"),
    "kaa": QCoreApplication.translate("Language", "喀拉卡尔帕克语"),
    "kk": QCoreApplication.translate("Language", "哈萨克语"),
    "kv": QCoreApplication.translate("Language", "科米语"),
    "ky": QCoreApplication.translate("Language", "吉尔吉斯语"),
    "lb": QCoreApplication.translate("Language", "卢森堡语"),
    "mk": QCoreApplication.translate("Language", "马其顿语"),
    "mhr": QCoreApplication.translate("Language", "草原马里语"),
    "mo": QCoreApplication.translate("Language", "摩尔多瓦语"),
    "os": QCoreApplication.translate("Language", "奥塞梯语"),
    "qu": QCoreApplication.translate("Language", "克丘亚语"),
    "rm": QCoreApplication.translate("Language", "罗曼什语"),
    "sd": QCoreApplication.translate("Language", "信德语"),
    "tg": QCoreApplication.translate("Language", "塔吉克语"),
    "tt": QCoreApplication.translate("Language", "鞑靼语"),
    "tyv": QCoreApplication.translate("Language", "图瓦语"),
    "udm": QCoreApplication.translate("Language", "乌德穆尔特语"),
    "sah": QCoreApplication.translate("Language", "萨哈语"),
}

# translate
translate_language_dict = {
    "en": QCoreApplication.translate("Language", "英语"),
    "zh": QCoreApplication.translate("Language", "中文"),
    "ja": QCoreApplication.translate("Language", "日语"),
    "ko": QCoreApplication.translate("Language", "韩语"),
    "fr": QCoreApplication.translate("Language", "法语"),
    "de": QCoreApplication.translate("Language", "德语"),
    "es": QCoreApplication.translate("Language", "西班牙语"),
    "pt": QCoreApplication.translate("Language", "葡萄牙语"),
    "ru": QCoreApplication.translate("Language", "俄语"),
    "ar": QCoreApplication.translate("Language", "阿拉伯语"),
    "it": QCoreApplication.translate("Language", "意大利语"),
    "nl": QCoreApplication.translate("Language", "荷兰语"),
    "hi": QCoreApplication.translate("Language", "印地语"),
    "tr": QCoreApplication.translate("Language", "土耳其语"),
    "vi": QCoreApplication.translate("Language", "越南语"),
    "th": QCoreApplication.translate("Language", "泰语"),
    "id": QCoreApplication.translate("Language", "印尼语"),
    "sv": QCoreApplication.translate("Language", "瑞典语"),
    "pl": QCoreApplication.translate("Language", "波兰语"),
    "el": QCoreApplication.translate("Language", "希腊语"),
    "cs": QCoreApplication.translate("Language", "捷克语"),
    "da": QCoreApplication.translate("Language", "丹麦语"),
    "fi": QCoreApplication.translate("Language", "芬兰语"),
    "no": QCoreApplication.translate("Language", "挪威语"),
    "hu": QCoreApplication.translate("Language", "匈牙利语"),
    "ro": QCoreApplication.translate("Language", "罗马尼亚语"),
    "uk": QCoreApplication.translate("Language", "乌克兰语"),
    "fa": QCoreApplication.translate("Language", "波斯语"),
    "he": QCoreApplication.translate("Language", "希伯来语"),
}

# AI模型
AI_model_dict = {
    "hunyuan-turbos-latest": QCoreApplication.translate("AI_Model", "腾讯混元"),
    "deepseek": QCoreApplication.translate("AI_Model", "Deepseek"),
    "gemini-3-flash-preview": QCoreApplication.translate("AI_Model", "Gemini 3 Flash"),
    "intern-latest": QCoreApplication.translate("AI_Model", "书生"),
    "glm-4.5-flash": QCoreApplication.translate("AI_Model", "GLM-4.5-FLASH"),
    "spark-lite": QCoreApplication.translate("AI_Model", "Spark-Lite"),
    "ernie-speed-128k": QCoreApplication.translate("AI_Model", "百度ERNIE-Speed-128K"),
    "custom-model": QCoreApplication.translate("AI_Model", "自定义模型"),
}
AI_ERROR_MAP = {
    # --- 身份验证与权限 ---
    "invalid_api_key": "API密钥无效，请检查设置",
    "authentication": "认证失败，请检查API Key是否正确",
    "unauthorized": "未授权访问，密钥可能已过期",
    "permission": "账号权限不足，请确认模型访问权限",
    # --- 余额与额度 ---
    "insufficient_quota": "账户额度不足，请及时充值",
    "quota": "额度已耗尽或账号已欠费",
    "balance": "账户余额不足",
    "credit_limit": "已达到信用额度限制",
    # --- 请求频率与并发 ---
    "rate_limit": "请求频率过快（RPM），请稍后重试",
    "too_many_requests": "并发请求数过多（TPM），请稍后重试",
    "concurrency": "已达到最大并行任务数限制",
    "429": "请求过于频繁，触发流量控制",
    # --- 内容审核 (安全限制) ---
    "policy": "内容触发安全审核政策，无法处理",
    "sensitive": "包含敏感词汇，请求被拦截",
    "safety": "内容因安全风险被过滤器拦截",
    "filtered": "输出内容因合规性被过滤",
    # --- 服务器状态 ---
    "overloaded": "服务器负载过高，请稍后重试",
    "busy": "服务器繁忙，请稍后重试",
    "internal": "服务器内部错误，请联系厂商支持",
    "server": "后端服务异常",
    "upstream": "上游服务报错",
    "500": "服务器崩溃，请稍后再试",
    "503": "服务暂时不可用，可能正在维护",
    # --- 网络与超时 ---
    "timeout": "请求超时，网络不稳定或响应过慢",
    "connection": "网络连接失败，请检查代理或网络设置",
    "connect": "无法连接到 API 服务器",
    "proxy": "代理服务器配置错误或连接断开",
    # --- 参数与模型 ---
    "invalid_request": "请求参数有误，请检查设置",
    "model_not_found": "指定的模型不存在或已被下线",
    "context_length": "内容超出模型最大上下文长度限制",
    "bad_request": "无效请求，请检查输入格式",
}

# 分区数据（参考 biliup.github.io/tid-ref.html）
# 160,生活 4,游戏 5,娱乐 36,知识 181,影视 3,音乐 1,动画 155,时尚 211,美食 223,汽车 234,运动 188,科技 217,动物圈 129,舞蹈 167,国创 119,鬼畜 177,纪录片 13,番剧 11,电视剧 23,电影
tid_data = {
    "生活": 160,
    "游戏": 4,
    "娱乐": 5,
    "知识": 36,
    "影视": 181,
    "音乐": 3,
    "动画": 1,
    "时尚": 155,
    "美食": 211,
    "汽车": 223,
    "运动": 234,
    "科技": 188,
    "动物圈": 217,
    "舞蹈": 129,
    "国创": 167,
    "鬼畜": 119,
    "纪录片": 177,
    "番剧": 13,
    "电视剧": 11,
    "电影": 23,
}

if sys.platform == "win32":
    EXE_SUFFIX = ".exe"
else:
    EXE_SUFFIX = ""
