# coding: utf-8
import sys
from pathlib import Path

AUTHOR = "baby2016"
TEAM = "天机阁(Fairy-Oracle-Sanctuary)"
VERSION = "2.2.0"
YEAR = "2025"
UPDATE_TIME = "2026-6-30"
if sys.platform == "win32":
    COPYLEFT = "🄯 "
else:
    COPYLEFT = "©️ "

RELEASE_URL = "https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases"
GITHUB_URL = "https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop"

CONFIG_FOLDER = Path("AppData").absolute()

CONFIG_FILE = CONFIG_FOLDER / "config.json"
DB_PATH = CONFIG_FOLDER / "database.db"

COVER_FOLDER = CONFIG_FOLDER / "Cover"
COVER_FOLDER.mkdir(exist_ok=True, parents=True)

PIC_SUFFIX = ".jpg"

# videocr
videocr_languages_dict = {
    "ch": "中文与英文",
    "chinese_cht": "繁体中文",
    "en": "英语",
    "japan": "日语",
    "korean": "韩语",
    "fr": "法语",
    "german": "德语",
    "es": "西班牙语",
    "pt": "葡萄牙语",
    "it": "意大利语",
    "ru": "俄语",
    "ar": "阿拉伯语",
    "nl": "荷兰语",
    "el": "希腊语",
    "sv": "瑞典语",
    "no": "挪威语",
    "da": "丹麦语",
    "fi": "芬兰语",
    "pl": "波兰语",
    "cs": "捷克语",
    "hu": "匈牙利语",
    "ro": "罗马尼亚语",
    "bg": "保加利亚语",
    "rs_cyrillic": "塞尔维亚语(西里尔文)",
    "rs_latin": "塞尔维亚语(拉丁文)",
    "hr": "克罗地亚语",
    "sk": "斯洛伐克语",
    "sl": "斯洛文尼亚语",
    "uk": "乌克兰语",
    "be": "白俄罗斯语",
    "sq": "阿尔巴尼亚语",
    "et": "爱沙尼亚语",
    "lv": "拉脱维亚语",
    "lt": "立陶宛语",
    "is": "冰岛语",
    "ga": "爱尔兰语",
    "cy": "威尔士语",
    "mt": "马耳他语",
    "hi": "印地语",
    "ur": "乌尔都语",
    "bh": "孟加拉语",
    "ta": "泰米尔语",
    "te": "泰卢固语",
    "mr": "马拉地语",
    "th": "泰语",
    "vi": "越南语",
    "id": "印度尼西亚语",
    "ms": "马来语",
    "tl": "菲律宾语",
    "fa": "波斯语",
    "tr": "土耳其语",
    "he": "希伯来语",
    "ne": "尼泊尔语",
    "si": "僧伽罗语",
    "my": "缅甸语",
    "km": "高棉语",
    "lo": "老挝语",
    "mn": "蒙古语",
    "ug": "维吾尔语",
    "uz": "乌兹别克语",
    "sw": "斯瓦希里语",
    "af": "南非荷兰语",
    "la": "拉丁语",
    "sa": "梵语",
    "mi": "毛利语",
    "abq": "阿巴扎语",
    "ady": "阿迪格语",
    "ang": "安吉卡语",
    "ava": "阿瓦尔语",
    "az": "阿塞拜疆语",
    "bho": "博杰普尔语",
    "bs": "波斯尼亚语",
    "che": "车臣语",
    "dar": "达尔格瓦语",
    "gom": "果阿康卡尼语",
    "bgc": "哈里亚纳语",
    "inh": "印古什语",
    "kbd": "卡巴尔达语",
    "ku": "库尔德语",
    "lbe": "拉克语",
    "lez": "列兹金语",
    "mah": "马加希语",
    "mai": "迈蒂利语",
    "sck": "那格浦尔语",
    "new": "尼瓦尔语",
    "oc": "奥克语",
    "pi": "巴利语",
    "tab": "塔巴萨兰语",
    "bal": "俾路支语",
    "ba": "巴什基尔语",
    "eu": "巴斯克语",
    "bua": "布里亚特语",
    "ca": "加泰罗尼亚语",
    "gl": "加利西亚语",
    "ka": "格鲁吉亚语",
    "xal": "卡尔梅克语",
    "kaa": "喀拉卡尔帕克语",
    "kk": "哈萨克语",
    "kv": "科米语",
    "ky": "吉尔吉斯语",
    "lb": "卢森堡语",
    "mk": "马其顿语",
    "mhr": "草原马里语",
    "mo": "摩尔多瓦语",
    "os": "奥塞梯语",
    "qu": "克丘亚语",
    "rm": "罗曼什语",
    "sd": "信德语",
    "tg": "塔吉克语",
    "tt": "鞑靼语",
    "tyv": "图瓦语",
    "udm": "乌德穆尔特语",
    "sah": "萨哈语",
}

# translate
translate_language_dict = {
    "en": "英语",
    "zh": "中文",
    "ja": "日语",
    "ko": "韩语",
    "fr": "法语",
    "de": "德语",
    "es": "西班牙语",
    "pt": "葡萄牙语",
    "ru": "俄语",
    "ar": "阿拉伯语",
    "it": "意大利语",
    "nl": "荷兰语",
    "hi": "印地语",
    "tr": "土耳其语",
    "vi": "越南语",
    "th": "泰语",
    "id": "印尼语",
    "sv": "瑞典语",
    "pl": "波兰语",
    "el": "希腊语",
    "cs": "捷克语",
    "da": "丹麦语",
    "fi": "芬兰语",
    "no": "挪威语",
    "hu": "匈牙利语",
    "ro": "罗马尼亚语",
    "uk": "乌克兰语",
    "fa": "波斯语",
    "he": "希伯来语",
}

# AI模型
AI_model_dict = {
    "hunyuan-turbos-latest": "腾讯混元",
    "deepseek": "Deepseek",
    "gemini-3.5-flash": "Gemini 3.5 Flash",
    "intern-latest": "书生",
    "glm-4.5-flash": "GLM-4.5-FLASH",
    "spark-lite": "Spark-Lite",
    "ernie-speed-128k": "百度ERNIE-Speed-128K",
    "custom-model": "自定义模型",
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
