// Internationalization messages
const messages = {
  zh: {
    "nav.features": "功能特性",
    "nav.workflow": "工作流程",
    "nav.guide": "使用指南",
    "nav.docs": "文档",
    "nav.download": "下载",
    "hero.badge": "开源 · 自由 · 一站式",
    "hero.title": '一站式<span class="accent underline">烤肉</span>自由软件',
    "hero.description": "从下载、字幕提取、语音识别、AI 翻译到视频压制，完整的视频字幕处理工作流。基于 PySide6 与 QFluentWidgets 打造的现代化桌面应用。",
    "hero.downloadButton": "立即下载",
    "hero.githubButton": "查看源码",
    "features.title": "核心功能",
    "features.subtitle": "覆盖烤肉全流程的强大工具集",
    "features.project.title": "项目管理",
    "features.project.description": "完整的项目文件系统管理，自动追踪封面、原视频、熟肉、字幕进度，支持从 YouTube 播放列表一键创建项目与批量任务派发。",
    "features.download.title": "视频下载",
    "features.download.description": "基于 yt-dlp，支持 1800+ 视频网站与播放列表批量下载，自动获取封面，可配置并发数、视频质量与格式。",
    "features.ocr.title": "字幕提取 OCR",
    "features.ocr.description": "集成 PaddleOCR，提供可视化字幕区域选择，支持上下双区域 OCR 与 GPU 加速，实时输出日志。",
    "features.whisper.title": "语音识别",
    "features.whisper.description": "基于 Const-me/Whisper，支持中、日、英、韩等多语种语音转字幕，带实时进度，支持 SRT / TXT / VTT 输出与 GPU 加速。",
    "features.translate.title": "智能翻译",
    "features.translate.description": "对接 Deepseek、腾讯混元、ERNIE、Gemini 等多个 AI 模型，可自定义提示词模板，支持深度思考模式与流式输出。",
    "features.ffmpeg.title": "视频压制",
    "features.ffmpeg.description": "基于 FFmpeg，自定义编码参数，支持 CUDA / VideoToolbox 硬件加速，自动嵌入字幕，实时输出日志。",
    "features.language.title": "多语言界面",
    "features.language.description": "支持 9 种语言：简体中文、英语、日语、韩语、德语、西班牙语、法语、葡萄牙语、繁体中文。",
    "workflow.title": "自动化工作流",
    "workflow.subtitle": "按顺序完成下载、提取、翻译、对轴和压制",
    "workflow.step1.title": "创建项目",
    "workflow.step1.desc": "新建项目，或者从 YouTube 播放列表导入视频，软件会自动建好文件夹。",
    "workflow.step2.title": "提取字幕",
    "workflow.step2.desc": "有硬字幕就用 OCR 提取，需要听写就用 Whisper 转字幕。",
    "workflow.step3.title": "翻译与对轴",
    "workflow.step3.desc": "用 AI 先翻译字幕，再手动校对、改时间轴，让字幕和画面重新对上。",
    "workflow.step4.title": "压制视频",
    "workflow.step4.desc": "把字幕嵌进视频，并按需要压小体积，方便保存和分享。",
    "guide.title": "快速上手",
    "guide.subtitle": "开箱即用，仅需配置 Whisper 模型即可释放完整本地算力",
    "guide.tab.sys": "系统要求",
    "guide.tab.prep": "模型下载",
    "guide.tab.run": "源码运行",
    "guide.sys.os": "<strong>操作系统：</strong>支持 Windows 与 macOS（暂不支持 Linux）。由于组件依赖，字幕提取 (OCR) 与语音识别 (Whisper) 仅支持 Windows 平台，其余功能在 macOS 上均可正常使用。",
    "guide.sys.python": "<strong>运行环境：</strong>Python 3.9 及以上版本，推荐使用 uv 进行极速依赖管理。",
    "guide.sys.gpu": "<strong>硬件加速：</strong>推荐配备 GPU，用于 OCR、Whisper 与视频压制加速；建议内存 8GB 以上。",
    "guide.prep.intro": "软件发布包已内置集成 FFmpeg、yt-dlp 与 PaddleOCR，开箱即用。唯独 Whisper 语音转文字模型由于文件体积较大，需要您手动下载并关联：",
    "guide.prep.download": "<strong>下载模型文件：</strong>前往迅雷网盘或 Hugging Face 下载所需的 ggml 格式 Whisper 模型。",
    "guide.prep.config": "<strong>关联本地路径：</strong>打开软件进入「设置」界面，在「Whisper -> 模型路径」中选中您下载的 .bin 模型文件。",
    "guide.prep.tips": "<strong>模型选择建议：</strong>短视频推荐使用 base 或 small 模型；10分钟以上推荐 medium，30分钟以上推荐 large 以防幻觉重复。",
    "guide.run.intro": "源码运行与部署流程（开发者/高级用户）：",
    "guide.run.code": "# 1. 克隆仓库\ngit clone https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop.git\ncd Fairy-Kekkai-Workshop\n\n# 2. 创建并激活虚拟环境\nuv venv\n.venv\\Scripts\\activate        # Windows\nsource .venv/bin/activate     # Unix/macOS\n\n# 3. 安装依赖\nuv pip install -r requirements.txt\n\n# 4. 从 Releases 下载 tools.zip，解压到项目根目录的 tools/ 文件夹\n#    https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases\n\n# 5. 运行应用\npython Fairy-Kekkai-Workshop.py",
    "download.title": "立即开始你的烤肉之旅",
    "download.desc": "完全本地化算力驱动，不收集任何视频内容与密钥隐私。",
    "download.win.detail": "完整支持 OCR、语音识别与硬件加速压制",
    "download.mac.detail": "支持视频下载、AI 翻译、项目管理及视频压制（暂不支持字幕提取与语音识别）",
    "download.btn": "获取最新安装包",
    "docs.nav.workflow": "工作流程",
    "docs.nav.requirements": "系统要求",
    "docs.nav.model": "Whisper 模型",
    "docs.nav.source": "源码运行",
    "docs.kicker": "Documentation",
    "docs.title": "使用文档",
    "docs.description": "这里整理了完整工作流程、平台支持、模型配置和源码运行方式。",
    "docs.requirements.title": "系统要求",
    "docs.model.title": "Whisper 模型下载",
    "docs.source.title": "源码运行",
    "footer.quote": "“连接世界，连接每一个相信自由与善意的人。”",
    "footer.github": "GitHub",
    "footer.discord": "Discord",
    "footer.copyright": "🄯 {year} 天机阁 Fairy Oracle Sanctuary"
  },
  en: {
    "nav.features": "Features",
    "nav.workflow": "Workflow",
    "nav.guide": "Guide",
    "nav.docs": "Docs",
    "nav.download": "Download",
    "hero.badge": "Open Source · Free · All-in-One",
    "hero.title": 'All-in-One <span class="accent underline">Subtitle</span> Workshop',
    "hero.description": "A complete video subtitle processing workflow — from downloading, subtitle extraction, speech recognition, and AI translation to video compression. A modern desktop app built with PySide6 and QFluentWidgets.",
    "hero.downloadButton": "Download Now",
    "hero.githubButton": "View Source",
    "features.title": "Core Features",
    "features.subtitle": "A powerful toolkit covering the entire localization workflow",
    "features.project.title": "Project Management",
    "features.project.description": "Complete project file system management with automatic progress tracking for covers, raw/cooked videos, and subtitles. Supports one-click project creation from YouTube playlists and batch task dispatch.",
    "features.download.title": "Video Download",
    "features.download.description": "Powered by yt-dlp, supporting 1800+ video sites and playlist batch downloads, automatic cover fetching, with configurable concurrency, quality, and format.",
    "features.ocr.title": "Subtitle OCR",
    "features.ocr.description": "Integrated PaddleOCR with visual subtitle area selection, dual-region OCR support, GPU acceleration, and real-time log output.",
    "features.whisper.title": "Speech Recognition",
    "features.whisper.description": "Based on Const-me/Whisper, supporting Chinese, Japanese, English, Korean and more, with real-time progress, SRT / TXT / VTT output, and GPU acceleration.",
    "features.translate.title": "Smart Translation",
    "features.translate.description": "Integrates multiple AI models including Deepseek, Tencent Hunyuan, ERNIE, and Gemini, with customizable prompt templates, deep reasoning mode, and streaming output.",
    "features.ffmpeg.title": "Video Compression",
    "features.ffmpeg.description": "Based on FFmpeg with custom encoding parameters, CUDA / VideoToolbox hardware acceleration, automatic subtitle embedding, and real-time log output.",
    "features.language.title": "Multi-language Interface",
    "features.language.description": "Supports 9 languages: Simplified Chinese, English, Japanese, Korean, German, Spanish, French, Portuguese, Traditional Chinese.",
    "workflow.title": "Automated Workflow",
    "workflow.subtitle": "Download, extract, translate, sync, and compress step by step",
    "workflow.step1.title": "Create Project",
    "workflow.step1.desc": "Create a project, or import videos from a YouTube playlist. The app will set up the folders for you.",
    "workflow.step2.title": "Extract Subtitles",
    "workflow.step2.desc": "Use OCR for hard subtitles, or use Whisper when you need speech-to-text subtitles.",
    "workflow.step3.title": "Translate & Sync",
    "workflow.step3.desc": "Use AI to translate first, then manually proofread and adjust timing so subtitles match the video.",
    "workflow.step4.title": "Compress Video",
    "workflow.step4.desc": "Burn subtitles into the video and reduce file size when needed, making it easier to store and share.",
    "guide.title": "Quick Start",
    "guide.subtitle": "Ready to run out-of-the-box, only the Whisper model requires manual download",
    "guide.tab.sys": "Requirements",
    "guide.tab.prep": "Model Download",
    "guide.tab.run": "Source Run",
    "guide.sys.os": "<strong>OS:</strong> Support Windows and macOS (Linux is currently not supported). Due to dependency requirements, Subtitle Extraction (OCR) and Speech Recognition (Whisper) are Windows-only, while other features run perfectly on macOS.",
    "guide.sys.python": "<strong>Runtime:</strong> Python 3.9 or higher, with uv recommended for fast dependency management.",
    "guide.sys.gpu": "<strong>Acceleration:</strong> A GPU is recommended for OCR, Whisper, and video compression; 8GB+ memory advised.",
    "guide.prep.intro": "The software release comes with integrated FFmpeg, yt-dlp, and PaddleOCR, fully out-of-the-box runnable. Only the Whisper speech-to-text model needs to be downloaded manually due to its large size:",
    "guide.prep.download": "<strong>Download Model File:</strong> Download the required ggml-format Whisper model from Xunlei Cloud Pan or Hugging Face.",
    "guide.prep.config": "<strong>Associate Local Path:</strong> Open the software, go to the \"Settings\" interface, and select the downloaded .bin model file in \"Whisper -> Model Path\".",
    "guide.prep.tips": "<strong>Model Selection:</strong> The base or small model is recommended for short videos; medium for 10+ minutes, and large for 30+ minutes to prevent hallucination repetitions.",
    "guide.run.intro": "Source code run and deployment flow (for developers/advanced users):",
    "guide.run.code": "# 1. Clone the repository\ngit clone https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop.git\ncd Fairy-Kekkai-Workshop\n\n# 2. Create and activate virtual environment\nuv venv\n.venv\\Scripts\\activate        # Windows\nsource .venv/bin/activate     # Unix/macOS\n\n# 3. Install dependencies\nuv pip install -r requirements.txt\n\n# 4. Download tools.zip from Releases and extract to the tools/ folder\n#    https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases\n\n# 5. Run the application\npython Fairy-Kekkai-Workshop.py",
    "download.title": "Start Your Localization Journey",
    "download.desc": "Fully local computing-driven, collecting no video content or key privacy.",
    "download.win.detail": "Full support for OCR, speech recognition, and hardware-accelerated compression",
    "download.mac.detail": "Supports video downloads, AI translation, project management, and video compression (OCR & Whisper are temporarily unsupported)",
    "download.btn": "Get Latest Installer",
    "download.mirror.text": "Can't access GitHub? Use our mirror download link",
    "download.mirror.btn": "Go to Mirror Download",
    "docs.nav.workflow": "Workflow",
    "docs.nav.requirements": "Requirements",
    "docs.nav.model": "Whisper Model",
    "docs.nav.source": "Run from Source",
    "docs.kicker": "Documentation",
    "docs.title": "Documentation",
    "docs.description": "Find the full workflow, platform support, model setup, and source code run guide here.",
    "docs.requirements.title": "Requirements",
    "docs.model.title": "Whisper Model Download",
    "docs.source.title": "Run from Source",
    "footer.quote": "\"Connect the world, connect every person who believes in freedom and goodwill.\"",
    "footer.github": "GitHub",
    "footer.discord": "Discord",
    "footer.copyright": "🄯 {year} Fairy Oracle Sanctuary"
  }
};

// Language names for display
const languageNames = {
  zh: "简体中文",
  en: "English"
};

// Current locale
let currentLocale = 'zh';

// Keys whose values contain HTML markup and must be set via innerHTML
const htmlKeys = new Set([
  'hero.title',
  'guide.sys.os', 'guide.sys.python', 'guide.sys.gpu',
  'guide.prep.intro', 'guide.prep.download', 'guide.prep.config', 'guide.prep.tips'
]);

// Showcase images per locale
const showcaseImages = {
  zh: {
    light: 'images/zh/thumbnail_full.png',
    dark:  'images/zh/thumbnail_full_black.png'
  },
  en: {
    light: 'images/en/thumbnail_full.png',
    dark:  'images/en/thumbnail_full_black.png'
  }
};

function updateShowcaseImages() {
  const imgLight = document.getElementById('showcase-light');
  const imgDark  = document.getElementById('showcase-dark');
  if (!imgLight || !imgDark) return;
  const imgs = showcaseImages[currentLocale] || showcaseImages.zh;
  imgLight.src = imgs.light;
  imgDark.src  = imgs.dark;
}

// Update all translations
function updateTranslations() {
  localStorage.setItem('locale', currentLocale);

  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.getAttribute('data-i18n');
    if (messages[currentLocale] && messages[currentLocale][key] !== undefined) {
      if (htmlKeys.has(key)) {
        el.innerHTML = messages[currentLocale][key];
      } else {
        el.textContent = messages[currentLocale][key];
      }
    }
  });

  // Update copyright with year
  const copyrightEl = document.getElementById('copyright');
  if (copyrightEl && messages[currentLocale]["footer.copyright"]) {
    const year = new Date().getFullYear();
    copyrightEl.textContent = messages[currentLocale]["footer.copyright"].replace('{year}', year);
  }

  // Update html lang attribute
  document.documentElement.lang = currentLocale;

  updateShowcaseImages();
}

// Particle background
function initParticles() {
  const container = document.getElementById('hero-particles');
  if (!container) return;

  const particleCount = 20;

  for (let i = 0; i < particleCount; i++) {
    const particle = document.createElement('div');
    particle.className = 'particle';

    const size = Math.random() * 4 + 2;
    particle.style.width = `${size}px`;
    particle.style.height = `${size}px`;
    particle.style.left = `${Math.random() * 100}%`;

    const duration = Math.random() * 10 + 15;
    particle.style.setProperty('--duration', `${duration}s`);

    const drift = (Math.random() - 0.5) * 200;
    particle.style.setProperty('--drift', `${drift}px`);

    particle.style.animationDelay = `${Math.random() * 20}s`;

    container.appendChild(particle);
  }
}

// Tabbed guide section
function initTabs() {
  const buttons = document.querySelectorAll('.tab-btn');
  const panels = document.querySelectorAll('.tab-panel');
  if (!buttons.length) return;

  buttons.forEach(btn => {
    btn.addEventListener('click', () => {
      const target = btn.dataset.tab;
      buttons.forEach(b => b.classList.toggle('active', b === btn));
      panels.forEach(p => p.classList.toggle('active', p.id === `tab-${target}`));
    });
  });
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
  const navEntry = performance.getEntriesByType?.('navigation')?.[0];
  const isReload = navEntry?.type === 'reload';
  if (isReload && window.location.hash) {
    history.replaceState(null, '', window.location.pathname + window.location.search);
  }

  // Element refs
  const langBtn = document.getElementById('langBtn');
  const langDropdown = document.getElementById('langDropdown');
  const currentLangSpan = document.getElementById('currentLang');
  const navToggle = document.getElementById('navToggle');
  const navMenu = document.getElementById('navMenu');
  const navbar = document.querySelector('.navbar');

  const closeNavMenu = () => {
    if (!navbar || !navToggle) return;
    navbar.classList.remove('nav-open');
    navToggle.classList.remove('open');
    navToggle.setAttribute('aria-expanded', 'false');
  };

  if (navMenu) {
    navMenu.addEventListener('click', (event) => {
      event.stopPropagation();
    });
  }

  if (navToggle && navMenu && navbar) {
    navToggle.addEventListener('click', (event) => {
      event.stopPropagation();
      const willOpen = !navbar.classList.contains('nav-open');
      navbar.classList.toggle('nav-open', willOpen);
      navToggle.classList.toggle('open', willOpen);
      navToggle.setAttribute('aria-expanded', String(willOpen));
      if (!willOpen && langDropdown) {
        langDropdown.classList.remove('show');
      }
    });

    navMenu.querySelectorAll('.nav-link').forEach(link => {
      link.addEventListener('click', () => {
        closeNavMenu();
      });
    });
  }

  if (langBtn && langDropdown) {
    langBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      langDropdown.classList.toggle('show');
    });

    document.addEventListener('click', () => {
      langDropdown.classList.remove('show');
      closeNavMenu();
    });

    const langOptions = langDropdown.querySelectorAll('.lang-option');
    langOptions.forEach(option => {
      option.addEventListener('click', () => {
        const newLocale = option.dataset.lang;
        if (newLocale !== currentLocale) {
          langOptions.forEach(opt => opt.classList.remove('active'));
          option.classList.add('active');
          currentLocale = newLocale;
          localStorage.setItem('locale', newLocale);
          if (currentLangSpan) currentLangSpan.textContent = languageNames[newLocale];
          updateTranslations();
        }
      });
    });
  }

  window.addEventListener('resize', () => {
    if (window.innerWidth >= 768) {
      closeNavMenu();
    }
  });

  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
      langDropdown?.classList.remove('show');
      closeNavMenu();
    }
  });

  // Detect browser language on first visit (default zh)
  const savedLocale = localStorage.getItem('locale');
  let detectedLocale = 'zh';

  if (savedLocale && messages[savedLocale]) {
    detectedLocale = savedLocale;
  } else {
    const browserLang = navigator.language.split('-')[0];
    if (messages[browserLang]) {
      detectedLocale = browserLang;
    }
  }

  currentLocale = detectedLocale;
  if (currentLangSpan) {
    currentLangSpan.textContent = languageNames[currentLocale];
  }
  if (langDropdown) {
    langDropdown.querySelectorAll('.lang-option').forEach(opt => {
      opt.classList.toggle('active', opt.dataset.lang === currentLocale);
    });
  }
  updateTranslations();

  // Theme toggle
  const themeToggle = document.getElementById('themeToggle');
  const savedTheme = localStorage.getItem('theme');

  if (savedTheme) {
    document.documentElement.setAttribute('data-theme', savedTheme);
  } else if (window.matchMedia('(prefers-color-scheme: dark)').matches) {
    document.documentElement.setAttribute('data-theme', 'dark');
  }

  if (themeToggle) {
    themeToggle.addEventListener('click', () => {
      const currentTheme = document.documentElement.getAttribute('data-theme');
      const newTheme = currentTheme === 'dark' ? 'light' : 'dark';
      document.documentElement.setAttribute('data-theme', newTheme);
      localStorage.setItem('theme', newTheme);
    });
  }

  // Particle background
  initParticles();

  // Tabbed guide
  initTabs();

  // Back to top button
  const backToTop = document.getElementById('backToTop');
  if (backToTop) {
    window.addEventListener('scroll', () => {
      if (window.scrollY > 300) {
        backToTop.classList.add('show');
      } else {
        backToTop.classList.remove('show');
      }
    });

    backToTop.addEventListener('click', () => {
      window.scrollTo({ top: 0, behavior: 'smooth' });
    });
  }
});
