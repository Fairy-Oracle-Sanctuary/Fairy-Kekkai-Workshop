#!/bin/bash
# Fairy-Kekkai-Workshop deb 包构建脚本
set -e

VERSION="2.0.0"
PACKAGE_NAME="fairy-kekkai-workshop_${VERSION}_amd64"
BUILD_DIR="$(pwd)/debian_package"
DEST_DIR="${BUILD_DIR}/opt/fairy-kekkai-workshop"

echo "=== 构建 Fairy-Kekkai-Workshop deb 包 v${VERSION} ==="

# 清理之前的构建
rm -rf "${BUILD_DIR}/opt"
rm -f "${BUILD_DIR}/../${PACKAGE_NAME}.deb"

# 创建目录结构
mkdir -p "${DEST_DIR}"
mkdir -p "${BUILD_DIR}/usr/bin"
mkdir -p "${BUILD_DIR}/usr/share/applications"
mkdir -p "${BUILD_DIR}/usr/share/icons/hicolor/256x256/apps"

# 复制应用代码（排除不需要的目录）
echo "复制应用代码..."
rsync -a --exclude='__pycache__' \
         --exclude='*.pyc' \
         --exclude='.git' \
         --exclude='.gitignore' \
         --exclude='AppData' \
         --exclude='tools' \
         --exclude='Output' \
         --exclude='dist' \
         --exclude='build' \
         --exclude='*.deb' \
         --exclude='debian_package' \
         --exclude='build-deb.sh' \
         --exclude='.venv' \
         --exclude='.venv_backup' \
         --exclude='thumbnail.png' \
         --exclude='*.md' \
         ./ "${DEST_DIR}/"

# 复制图标
echo "复制图标..."
cp "${DEST_DIR}/app/resource/images/logo.png" \
   "${BUILD_DIR}/usr/share/icons/hicolor/256x256/apps/fairy-kekkai-workshop.png"

# 设置启动脚本权限
chmod 755 "${BUILD_DIR}/usr/bin/fairy-kekkai-workshop"

# 设置 DEBIAN 脚本权限
chmod 755 "${BUILD_DIR}/DEBIAN/postinst"
chmod 755 "${BUILD_DIR}/DEBIAN/prerm"

# 计算包大小（KB，取整）
echo "计算包大小..."
SIZE=$(du -sk "${DEST_DIR}" | cut -f1)
sed -i "/^Installed-Size:/d" "${BUILD_DIR}/DEBIAN/control"
echo "Installed-Size: ${SIZE}" >> "${BUILD_DIR}/DEBIAN/control"

# 构建 deb 包
echo "构建 deb 包..."
if command -v dpkg-deb &>/dev/null; then
    fakeroot dpkg-deb --build "${BUILD_DIR}" "${PACKAGE_NAME}.deb"
else
    # 手动构建 deb（使用 ar + tar）
    cd "${BUILD_DIR}"

    # 创建 debian-binary
    echo "2.0" > debian-binary

    # 创建 control.tar.gz
    cd DEBIAN
    tar czf "${BUILD_DIR}/control.tar.gz" .
    cd "${BUILD_DIR}"

    # 创建 data.tar.gz（opt/ 和 usr/）
    tar czf "${BUILD_DIR}/data.tar.gz" opt/ usr/

    # 构建 deb 包
    ar rcs "$(pwd)/../${PACKAGE_NAME}.deb" debian-binary control.tar.gz data.tar.gz

    # 清理临时文件
    rm -f debian-binary control.tar.gz data.tar.gz

    cd ..
fi

echo ""
echo "=== 构建成功！==="
echo "包文件: $(pwd)/${PACKAGE_NAME}.deb"
echo ""
echo "安装命令:"
echo "  sudo dpkg -i ${PACKAGE_NAME}.deb"
echo "  sudo apt-get install -f  # 修复依赖"
echo ""
echo "卸载命令:"
echo "  sudo dpkg -r fairy-kekkai-workshop"
