# Python common/components → C++ parity tracker

Source of truth: `app/common` and `app/components`. A file is complete only after
its public behavior, signals, configuration, appearance, and call sites match.
The C++ code is not fully ported yet. Do not mark a placeholder as complete.

## Common

| Python module | C++ counterpart | Status / remaining work |
| --- | --- | --- |
| application.py | common/application.* | Shared-memory singleton, local IPC and second-instance window activation ported; check startup failure UI |
| config.py | common/config.* | Partial: 114 keys, 101 literal and 13 dynamic defaults, 22 ranges, 22 literal option sets, 28 booleans, restart flags and project config; 5 dynamic option sets and serializers remain |
| event_bus.py | common/event_bus.* | Signals declared; connect each producer and consumer |
| events.py | common/events.h | Data structures and builders ported; connect download dispatcher |
| logger.py | common/logger.* | Logger cache, UTF-8 files, ANSI stripping, event messages, old-log cleanup and log file path accessor ported; migrate call sites |
| paddleocr.py | common/paddleocr.* | Language groups and model-directory resolution ported from source AST; migrate OCR call sites |
| setting.py | common/setting.* | Literal constants and tables generated from Python AST; runtime paths, platform suffix and PaddleOCR version ported; migrate consumers |
| startup_handler.py | — | Missing startup log and exception handling |
| style_sheet.py | common/style_sheet.* | Theme-aware resource path and shared QSS application ported; sample cards use original QSS |
| task_status.py | common/task_status.h | Enum values and localized names ported; migrate tasks |
| text.py | common/text.*, common/text_format.*, tools/generate_text.py | 1024 translated fields generated from Python AST with original names and `Text` Qt context; lazy singleton, Python-style brace formatting, eight `.qm` resources and Windows user locale for Auto; library zh_CN translator and matching Python strings wired; untranslated placeholder text in unfinished pages remains and user build must verify |
| utils.py | common/utils.* | URL and explorer behavior ported; migrate call sites |

## Services

| Python module | C++ counterpart | Status / remaining work |
| --- | --- | --- |
| ffmpeg_service.py | service/ffmpeg_service.* | Ported 1:1: FFmpegTask data class (Easy-FFmpeg fields, shared_ptr reference semantics), worker-thread audio probing, pure output-format fix and command building, QRunnable worker with QEventLoop sync and DirectConnection stderr handling, duration re-summing/freeze, progress parsing throttled to 4/s, cancel/kill and event_bus status/finish reporting; task-interface dispatch and user build verification pending |
| ocr_service.py | service/ocr_service.* | Ported 1:1: `OcrTask` data class snapshotting every videocr parameter (lang/time range/GPU/dual-zone/angle-cls/server-model/similarity/merge-gap/SSIM/frames-skip/max-width/min-duration/confidence plus PaddleOCR/support-files/temp paths and the video-preview crop rects), pure `buildOcrCommand` emitting the videocr-cli argument order including single and second `--crop_*` zones, QRunnable `OcrWorker` with Logger naming, temp-dir cleanup, MergedChannels + DirectConnection stdout handling, `\r` normalization, per-line change-dedup for Step 1, the three-stage progress mapping (0-33 / 33-53 / 53-66 / 66-100), `taskLogSignal("videocr", …)` normal/error/flush routing, `taskkill /F /T /PID` cancel with 2s fallback kill, `原文.srt` activation (only for 生肉.mp4 when absent) and event_bus status/finish reporting, plus `ScreenOcrRunner` (main-thread `grabWindow` capture, `resolveModelDirs` command build, `PYTHONIOENCODING`/`PYTHONUNBUFFERED` environment, streamed `ppocr INFO` parsing via two regexes replacing `ast.literal_eval`, `screen_ocr_*` reporting). `view/videocr_task_interface.*` overrides createTask/createWorker/type-text/generated-files(+原文.srt)/legacy-finished/log-channel (videocr) and carries `setCropRects`, while `view/videocr_interface.*` adds `cropRects()`, the extraction-page log box with Python-identical `_log_message` semantics (hh:mm:ss prefix, red error lines, flush replacing the previous line, auto-scroll to bottom, clear-logs button) fed by the `videocr` `taskLogSignal` channel plus local video-load and custom-area lines, the pre-start `validateBeforeStart` port (missing PaddleOCR exe, CJK-path warnings for PaddleOCR/support-files/temp-dir, missing support files, empty input/output, empty selection) and forwards preview selections into the queue. Floating OCR window and user build verification pending |
| translate_service.py | service/translate_service.* | Ported 1:1: SRT parse/assemble and `<thinking>...</thinking>` stripping, eight-provider OpenAI-compatible resolution (incl. custom-model base-URL normalization and endpoint preference, Deepseek model taken from the task snapshot), Qt streaming SSE client (`QNetworkAccessManager` + `QEventLoop`, cancel-aware abort via watchdog timer, no libcurl), 50-item batching, 2 retries, JSON/XML/numbered multi-strategy response parsing, SRT-safe text sanitizing, post-process thinking removal and event_bus status/log/finish reporting. `view/translate_task_interface.*` overrides createTask/createWorker/legacy-finished/generated-files, and `view/translate_interface.*` binds the language/model/context/Deepseek cards to config with pre-start API-key validation. The floating-window one-shot `ScreenTranslateThread` is deferred with the unported floating OCR window; user build verification pending |
| whisper_service.py | service/whisper_service.* | Ported 1:1: WhisperTask data class (model/language/format/gpu snapshot), `getWhisperCliPath` custom-path-with-repo-fallback, pure command building (`-f/-l/-osrt/-otxt/-ovtt/-gpu/-m`), worker-thread duration probing via ffmpeg stderr, QRunnable worker with QEventLoop sync and MergedChannels/DirectConnection stdout handling, timestamp-line progress with change-dedup, error-line routing and `taskLogSignal` flush/error/normal channels, cancel/kill (cancelled tasks emit no finish), output activation (rename side-car output to task output, copy to 原文.srt only for 生肉.mp4 when absent). `view/whisper_task_interface.*` overrides createTask/createWorker/type-text/generated-files(+原文.srt)/legacy-finished, and `view/whisper_interface.*` binds language/format cards to config, validates CLI/model paths before start (WCPDNE/WMPDNE), shows the rich-text model hint and consumes `whisper_requested`/`whisper_video_load_signal`; user build verification pending |
| version_service.py | common/version_service.*, components/update_dialog.* | Latest release check, version comparison, changelog, OCR installer selection, asynchronous installer download and folder reveal ported; user build and live release verification pending |

## Components

| Python module | C++ counterpart | Status / remaining work |
| --- | --- | --- |
| base_function_interface.py | components/base_function_interface.* | Partial; command area and signals missing. Adds a `validateBeforeStart` hook (used by the translation page for API-key checks) and `special_filename_mapping` (translate maps 原文.srt→译文.srt) |
| base_stacked_interface.py | components/base_stacked_interface.* | Partial; compare navigation behavior |
| base_task_card.py | — | Missing base task card, delete dialog, lifecycle |
| base_task_interface.py | components/base_task_interface.* | Generic queue ported (QThreadPool + event_bus + CommandBar): task dedup, filters, selection, delete/retry/cancel, notifications, concurrency from config, and `createTask`/`createWorker`/`getTaskPath`/`taskGeneratedFiles`/`emitLegacyFinished` extension points now decoupled from FFmpeg via `TaskBase`/`TaskWorker`, plus a `logName` hook that routes task-done/failed lines into the page log box; FFmpeg, Translate, Whisper and OCR wired, OCR page log box live; user build verification pending |
| config_card.py | components/config_card.* | OCR, yt-dlp, Whisper, FFmpeg, AI translation and upload settings cards now mirror the visible Python groups and use shared AppConfig keys; custom model check is asynchronous. GPU toggle is forced by the root `PADDLEOCR` marker (CPU build locks it off, GPU build locks it on — PaddleOCR 3.7.0 GPU exe cannot run PP-OCRv6 on CPU). Upload settings are not yet mounted in a C++ upload page; user build and visual verification pending |
| dialog.py | components/dialog.* | Placeholder; most of 14 dialogs missing |
| download_card.py | — | Missing download task widget |
| empty_status_widget.py | components/empty_status_widget.* | Compare states and text |
| floating_window.py | — | Missing floating OCR window and selection overlay; its `ScreenOCRThread` counterpart `ScreenOcrRunner` already exists in `service/ocr_service.*` and is ready to be wired once the window lands |
| infobar.py | components/notification_service.h | Four InfoBar types, timing and placement ported; integrate other call sites |
| info_card.py | components/info_card.* | Partial |
| pager.py | components/pager.* | Page buttons, ellipses, jump and theme behavior ported; bottom project pager wired |
| project_card.py | components/project_card.*, common/project_service.* | Core actions and drag reorder wired; adds a health badge and one-click repair button (see Intelligent repair); flyout styling and user verification remain |
| sample_card.py | components/sample_card.* | Partial; compare all button variants |
| screen.py | components/screen.* | Screen and geometry functions ported |
| statistic_widget.py | components/statistic_widget.* | Compare values and updates |
| system_tray.py | components/system_tray.* | Tray menu and close-to-tray behavior wired; task-aware quit still missing |
| task_card.py | components/task_card.* | Generic TaskCard ported (icon from `TaskBase::iconName`, status/progress/finished info, folder/cancel/retry/log/delete actions, selection mode, delete dialog); the four Python task cards collapse into this one card driven by `PreviewTaskKind`; user build and visual verification pending |
| teaching_tips.py | — | Missing guided tour; settings button currently shows a short info tip |
| tool_tip.py | Qt Fluent Widgets tooltip | Check all positions, delegates and per-item data |

## Project management

The C++ project service now reads Python's `AppData/project.json` links/order and
`标题.txt` episode records. The list supports create, copy/link import, edit,
rename, icon selection, pin, drag reorder, unlink, deletion, progress, and
playlist metadata creation through asynchronous yt-dlp. The detail view supports
insert/delete/edit episode, local file import, setting current subtitle, file
delete, and checked batch delete. User compilation and visual verification pending.

Still pending: playlist auto-download/cover generation; batch task dispatch;
per-file download, OCR, Whisper, translation, and encoding actions. These depend
on the unfinished C++ task queue and its signal consumers. Large copy/delete
operations currently run on the GUI thread and need background workers.

## Intelligent repair (new, not in Python)

`common/project_service.*` now ships a tolerant recognizer plus
`service/project_health.*`, and the project list is built from
`projects::candidates()` instead of the strict `projects::paths()`. A project is
still listed when its episode folders are missing a number, when `标题.txt` was
renamed or rewritten by hand, or when the project directory itself is gone.
When `标题.txt` was deleted outright the folder is accepted only if its numeric
folders form an unbroken `1..n`; a layout such as `1 2 4` is rejected so that
unrelated folders are not mistaken for projects.
Damaged cards show a health badge and a one-click repair button. Structural
damage (`标题.txt` missing/renamed/broken, missing episode folders, folders
without records, project folder gone) blocks opening until repaired, while a
missing marker file, a missing icon or leftover `.deleted-` folders only show a
"可优化" hint and never block. `projects::repair()` backs up `标题.txt`, fills
missing episode folders with empty directories, appends title records for
folders that have none, then re-reads the result and rolls back on failure.
C++ syntax check passes; user compilation and visual verification pending.

## Project root relocation (new, not in Python)

Python always created projects inside the software directory, so uninstalling or
cleaning the program folder destroyed user data. The C++ port now resolves a
separate library root: `projects::root()` reads the remembered `project_root`
from `project.json`, and falls back to a parallel directory next to the software
folder (`<drive>:/Fairy-Kekkai-Workshop-Projects`, `D:` first then `C:`), then
the system Documents folder, then the per-user data folder. The resolved
path is guaranteed never to be the software directory nor to contain it, and
`projects::checkRoot()` additionally requires a candidate to be an existing
**empty** folder. `localPath()`, `paths()`, `candidates()` and the playlist
creation flow now build from this root; `remove()` still accepts the software
directory so pre-existing projects do not become undeletable.

`service/project_relocate.*` performs the move inside a `QThread` worker:
a same-volume `QDir::rename` when possible, otherwise a chunked copy followed
by a per-file size verification before the source is removed, with the partial
target cleaned up and the byte counter rolled back on any failure. Name clashes
are resolved with `-2`, `-3`, ... suffixes instead of overwriting. Progress is
weighted by the pre-scanned total byte count.
`components/project_migration.*` wraps this in a modal dialog that shows the
current item, a progress bar and a per-project log; it disables the close button
and ignores `reject()` while running, so the user can only continue after the
move finishes. `main.cpp` triggers it at startup when `projectsIn(sourceRoot())`
is non-empty (skipped for the `--capture-*` automation flags), and the settings
page adds a 项目目录 card that accepts only an empty folder and immediately
migrates the existing projects into it. After a move, `applyRelocation()`
rewrites the link and order tables, and registers any project that could not be
moved as a link so it stays visible. User compilation and visual verification
pending.

## Startup maintenance (new, not in Python)

`service/ocr_migration.*` guards the OCR toolchain against an update that ships a
different PaddleOCR build. On startup `fixOcrPaths()` reads the version from the
`PADDLEOCR` marker in the software root (`PaddleOCR-GPU-v3.7.0-CUDA-12.9`) and
rewrites `OCR/PaddleocrPath` and `OCR/supportFilesPath` when the stored value
points at a superseded directory (or at a target that no longer exists); a missing
or unreadable marker leaves the configuration untouched, so a mis-detected version
can never damage a working setup. `setting.h` owns the three helpers
(`paddleOcrDefaultPath()`, `paddleOcrSupportFilesName()`,
`paddleOcrSupportFilesDefaultPath()`) that both the dynamic defaults in
`config.cpp` and this check share, and `paddleocr.cpp` derives the PP-OCRv6 /
PP-OCRv5-fallback model names from the same source.

`hasObsoleteResources()` / `scanObsoleteResources()` list what the previous
release put in the software folder, taking the names from the release workflow
(`.github/workflows/deploy-windows.yml`, mirrored by `release.yml`): the engine
is downloaded as `PaddleOCR-{CPU,GPU}-v1.4.0[-CUDA-*].7z` and moved to
`tools/PaddleOCR-{CPU,GPU}-v1.5.1[-CUDA-*]`, and the model archive
`PaddleOCR.PP-OCRv5.support.files.VideOCR.7z` is expanded to
`tools/PaddleOCR.PP-OCRv5.support.files`. Those `v1.5.1` / `PP-OCRv5` names, the
matching downloaded archives, the retired `PaddleOCR.font.support.files` and any
older sibling matching `PaddleOCR-{CPU,GPU}-*` / `PaddleOCR.PP-OCRv*.support.files`
are the only candidates, and any name equal to the current version's engine or
model directory is skipped first. The scan covers one level of the software root,
`tools/` and `downloads/`; anything else there (whisper, ffmpeg, videocr-cli, ...)
is never considered, deletion is restricted to entries living inside the software
directory, and nothing is deleted at all while the version marker is unreadable —
so the current version's own PaddleOCR and model directory are always preserved.
`ResourceCleaner` performs the scan inside its worker thread (so a multi-gigabyte
directory is never sized on the UI thread) and reports per-item progress weighted
by released bytes.

`components/startup_maintenance.*` merges this with the legacy project relocation
into a single modal dialog: `runStartupMaintenance()` returns without any window
when neither is needed, otherwise a `StartupMaintenanceWorker` runs both phases
sequentially on one `QThread` while the dialog shows the current item, a shared
progress bar (project phase 0-55%, OCR phase 55-100% when both run, byte-weighted
within each phase) and a per-item log. The dialog ignores `reject()` and hides the
cancel button while running, so a half-moved project library or a half-deleted
resource directory cannot be left behind. `main.cpp` calls it at startup in place
of the former `migrateProjects()` + `startOcrCleanup()` pair and, when projects
were moved, feeds the report back through `applyRelocation()` and refreshes the
project list. `migrateProjects()` remains for the settings page's manual folder
switch. User compilation and visual verification pending.

## Integration order

1. Complete config schema, validation, events, notifications, logging and runtime paths.
2. Complete reusable controls: task cards, dialogs, pager, tooltips and tray.
3. Wire pages to these components and compare their behavior with Python.
4. User performs C++ compilation and visual verification; resolve reported issues.
