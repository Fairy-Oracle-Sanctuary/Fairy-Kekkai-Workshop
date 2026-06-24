#ifdef _WIN32
#include <windows.h>
#endif
#include "cvutils_api.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Error callback
// ---------------------------------------------------------------------------
static std::string g_last_error;
static void error_cb(const char *msg) {
  g_last_error = msg ? msg : "";
  std::cerr << "[CVUtils Error] " << msg << std::endl;
}

// ---------------------------------------------------------------------------
// Simple arg parser: key-value pairs after the command name
// ---------------------------------------------------------------------------
static std::map<std::string, std::string> parse_args(int argc, char *argv[],
                                                     std::string &out_cmd) {
  std::map<std::string, std::string> args;
  if (argc < 2)
    return args;
  out_cmd = argv[1];
  for (int i = 2; i < argc; ++i) {
    std::string key = argv[i];
    if (key.rfind("--", 0) == 0 && i + 1 < argc) {
      args[key] = argv[i + 1];
      ++i;
    }
  }
  return args;
}

// ---------------------------------------------------------------------------
// Load image -> cv::Mat (RGB, matching Luna Python binding)
// ---------------------------------------------------------------------------
static cv::Mat load_image_rgb(const fs::path &path) {
  cv::Mat bgr = cv::imread(path.string(), cv::IMREAD_COLOR);
  if (bgr.empty())
    return {};
  cv::Mat rgb;
  cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
  return rgb;
}

// ---------------------------------------------------------------------------
// Run OCR on one image, collect results
// ---------------------------------------------------------------------------
struct OcrResult {
  std::array<cv::Point2f, 4> box;
  std::string text;
  float confidence;
};

static std::vector<OcrResult> run_ocr_on_image(void *ocr, const cv::Mat &rgb) {
  std::vector<OcrResult> results;
  auto cb = [](float x1, float y1, float x2, float y2, float x3, float y3,
               float x4, float y4, const char *text) {
    auto *vec = static_cast<std::vector<OcrResult> *>(
        static_cast<void *>(const_cast<char *>(text))); // hack to pass userdata
  };
  // Use a thread_local or global to receive callbacks since CVUtils API doesn't
  // pass userdata
  static std::vector<OcrResult> *g_results = nullptr;
  g_results = &results;

  auto callback = [](float x1, float y1, float x2, float y2, float x3, float y3,
                     float x4, float y4, float conf, const char *text) {
    OcrResult r;
    r.box = {cv::Point2f(x1, y1), cv::Point2f(x2, y2), cv::Point2f(x3, y3),
             cv::Point2f(x4, y4)};
    r.text = text ? text : "";
    r.confidence = conf;
    g_results->push_back(r);
  };

  OcrDetect(ocr, &rgb, Directional::H, callback, error_cb);
  return results;
}

// ---------------------------------------------------------------------------
// Format polygon for JSON output
// ---------------------------------------------------------------------------
static std::string json_escape(const std::string &s) {
  std::ostringstream oss;
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      oss << "\\\"";
      break;
    case '\\':
      oss << "\\\\";
      break;
    case '\b':
      oss << "\\b";
      break;
    case '\f':
      oss << "\\f";
      break;
    case '\n':
      oss << "\\n";
      break;
    case '\r':
      oss << "\\r";
      break;
    case '\t':
      oss << "\\t";
      break;
    default:
      if (c < 0x20) {
        const char *hex = "0123456789abcdef";
        oss << "\\u00" << hex[(c >> 4) & 0x0f] << hex[c & 0x0f];
      } else {
        oss << static_cast<char>(c);
      }
      break;
    }
  }
  return oss.str();
}

static std::string format_poly_json(const std::array<cv::Point2f, 4> &box) {
  std::ostringstream oss;
  oss << "[[" << box[0].x << "," << box[0].y << "],[" << box[1].x << ","
      << box[1].y << "],[" << box[2].x << "," << box[2].y << "],[" << box[3].x
      << "," << box[3].y << "]]";
  return oss.str();
}

// ---------------------------------------------------------------------------
// Command: text_detection
//   paddleocr.exe text_detection
//      --input <dir>
//      --model_dir <dir>
//      --model_name <name>
//      [--device cpu|gpu]
// ---------------------------------------------------------------------------
static int cmd_text_detection(const std::map<std::string, std::string> &args) {
  fs::path input_dir = args.count("--input") ? args.at("--input") : "";
  fs::path model_dir = args.count("--model_dir") ? args.at("--model_dir") : "";
  std::string device = args.count("--device") ? args.at("--device") : "cpu";
  bool use_gpu = (device == "gpu");
  // Map CLI device flag to ONNX Runtime provider name
  std::string provider = use_gpu ? "DML" : "CPU";

  if (input_dir.empty() || model_dir.empty()) {
    std::cerr << "Usage: text_detection --input <dir> --model_dir <dir> "
                 "--model_name <name> [--device cpu|gpu]"
              << std::endl;
    return 1;
  }

  fs::path det_onnx = model_dir / "det.onnx";
  fs::path rec_onnx = model_dir / "rec.onnx";
  fs::path dict_txt = model_dir / "dict.txt";

  if (!fs::exists(det_onnx) || !fs::exists(rec_onnx) || !fs::exists(dict_txt)) {
    std::cerr << "Model files not found in " << model_dir << std::endl;
    return 1;
  }

  if (!OcrLoadRuntime()) {
    std::cerr << "Failed to load ONNX Runtime" << std::endl;
    return 1;
  }

  g_last_error.clear();
  void *ocr = OcrInit(det_onnx.wstring().c_str(), rec_onnx.wstring().c_str(),
                      dict_txt.wstring().c_str(), 4, use_gpu, 0, provider.c_str(), error_cb);
  if (!ocr) {
    std::cerr << "OcrInit failed: " << g_last_error << std::endl;
    return 1;
  }

  std::vector<fs::path> images;
  for (const auto &entry : fs::directory_iterator(input_dir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp")
        images.push_back(entry.path());
    }
  }
  std::sort(images.begin(), images.end());

  int processed = 0;
  for (const auto &img_path : images) {
    cv::Mat rgb = load_image_rgb(img_path);
    if (rgb.empty())
      continue;

    std::vector<OcrResult> results;
    try {
      results = run_ocr_on_image(ocr, rgb);
    } catch (...) {
      std::cerr << "Error on image index " << (processed + 1) << ": skipped" << std::endl;
      ++processed;
      continue;
    }

    std::ostringstream json;
    std::string path_str = img_path.string();
    std::replace(path_str.begin(), path_str.end(), '\\', '/');
    json << "{\"input_path\":\"" << json_escape(path_str) << "\",";
    json << "\"dt_polys\":[";
    for (size_t i = 0; i < results.size(); ++i) {
      if (i)
        json << ",";
      json << format_poly_json(results[i].box);
    }
    json << "],\"dt_scores\":[";
    for (size_t i = 0; i < results.size(); ++i) {
      if (i)
        json << ",";
      json << results[i].confidence;
    }
    json << "]}";

    std::cout << json.str() << std::endl;

    ++processed;
    std::cout << "ppocr INFO: Processed item " << processed << "/"
              << images.size() << std::endl;
  }

  OcrDestroy(ocr);
  return 0;
}

// ---------------------------------------------------------------------------
// Command: ocr
//   paddleocr.exe ocr
//      --input <dir>
//      --device cpu|gpu
//      --use_textline_orientation true|false
//      --lang <lang>
//      --text_detection_model_dir <dir>
//      --text_recognition_model_dir <dir>
//      [--textline_orientation_model_dir <dir>]
//      [--output <file>]
// ---------------------------------------------------------------------------
static int cmd_ocr(const std::map<std::string, std::string> &args) {
  fs::path input_dir = args.count("--input") ? args.at("--input") : "";
  fs::path det_dir = args.count("--text_detection_model_dir")
                         ? args.at("--text_detection_model_dir")
                         : "";
  fs::path rec_dir = args.count("--text_recognition_model_dir")
                         ? args.at("--text_recognition_model_dir")
                         : "";

  if (input_dir.empty() || det_dir.empty() || rec_dir.empty()) {
    std::cerr
        << "Usage: ocr --input <dir> --device <cpu|gpu> --lang <lang> "
           "--text_detection_model_dir <dir> --text_recognition_model_dir <dir>"
        << std::endl;
    return 1;
  }

  fs::path det_onnx = det_dir / "det.onnx";
  fs::path rec_onnx = rec_dir / "rec.onnx";
  fs::path dict_txt = rec_dir / "dict.txt";

  if (!fs::exists(det_onnx) || !fs::exists(rec_onnx) || !fs::exists(dict_txt)) {
    std::cerr << "Model files not found" << std::endl;
    return 1;
  }

  bool use_gpu = false;
  if (args.count("--device"))
    use_gpu = (args.at("--device") == "gpu");

  std::string provider = use_gpu ? "DML" : "CPU";

  if (!OcrLoadRuntime()) {
    std::cerr << "Failed to load ONNX Runtime" << std::endl;
    return 1;
  }

  g_last_error.clear();
  void *ocr =
      OcrInit(det_onnx.wstring().c_str(), rec_onnx.wstring().c_str(),
              dict_txt.wstring().c_str(), 4, use_gpu, 0, provider.c_str(), error_cb);
  if (!ocr) {
    std::cerr << "OcrInit failed: " << g_last_error << std::endl;
    return 1;
  }

  std::vector<fs::path> images;
  for (const auto &entry : fs::directory_iterator(input_dir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp")
        images.push_back(entry.path());
    }
  }
  std::sort(images.begin(), images.end());

  std::string output_file;
  auto it = args.find("--output");
  if (it != args.end())
    output_file = it->second;
  std::ofstream out_json;
  if (!output_file.empty()) {
    out_json.open(output_file);
    if (!out_json.is_open()) {
      std::cerr << "Failed to open output file: " << output_file << std::endl;
      return 1;
    }
  }

  int processed = 0;
  for (const auto &img_path : images) {
    cv::Mat rgb = load_image_rgb(img_path);
    if (rgb.empty())
      continue;

    std::vector<OcrResult> results;
    try {
      results = run_ocr_on_image(ocr, rgb);
    } catch (...) {
      std::cerr << "Error on image index " << (processed + 1) << ": skipped" << std::endl;
      ++processed;
      continue;
    }

    std::ostringstream result_json;
    result_json << "{\"image\":\"" << json_escape(img_path.filename().string())
                << "\",\"results\":[";
    for (size_t i = 0; i < results.size(); ++i) {
      if (i)
        result_json << ",";
      result_json << "{\"box\":" << format_poly_json(results[i].box)
                  << ",\"text\":\"" << json_escape(results[i].text) << "\",\"score\":" << results[i].confidence << "}";
    }
    result_json << "]}";

    if (out_json.is_open()) {
      out_json << result_json.str() << std::endl;
    } else {
      std::cout << result_json.str() << std::endl;
    }

    ++processed;
    std::cout << "ppocr INFO: Processed item " << processed << "/"
              << images.size() << std::endl;
  }

  if (out_json.is_open())
    out_json.close();

  OcrDestroy(ocr);
  return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::string cmd;
  auto args = parse_args(argc, argv, cmd);

  if (cmd == "text_detection")
    return cmd_text_detection(args);
  if (cmd == "ocr")
    return cmd_ocr(args);

  std::cerr << "Unknown command: " << cmd << std::endl;
  std::cerr << "Supported commands: text_detection, ocr" << std::endl;
  return 1;
}
