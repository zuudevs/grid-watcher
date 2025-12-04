#!/bin/bash
# =============================================================================
# Grid-Watcher Release Build Script
# =============================================================================
# This script builds Grid-Watcher for production release with all optimizations
# enabled and creates distribution packages.
#
# Usage:
#   ./build-release.sh [OPTIONS]
#
# Options:
#   --version VERSION    Set version number (default: from git tag)
#   --platform PLATFORM  Build for specific platform (linux, windows, all)
#   --skip-tests         Skip running tests
#   --clean              Clean before build
#   -h, --help           Show this help message
#
# =============================================================================

set -e  # Exit on error
set -u  # Exit on undefined variable

# =============================================================================
# Configuration
# =============================================================================

PROJECT_NAME="grid-watcher"
BUILD_DIR="build-release"
DIST_DIR="dist"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default values
VERSION=""
PLATFORM="$(uname -s | tr '[:upper:]' '[:lower:]')"
SKIP_TESTS=false
CLEAN_BUILD=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# =============================================================================
# Helper Functions
# =============================================================================

print_header() {
    echo -e "${BLUE}"
    echo "============================================================================="
    echo "$1"
    echo "============================================================================="
    echo -e "${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

show_help() {
    head -n 20 "$0" | tail -n +3 | sed 's/^# //g' | sed 's/^#//g'
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_error "$1 is required but not installed"
        exit 1
    fi
}

get_version() {
    if [ -n "$VERSION" ]; then
        echo "$VERSION"
    elif git describe --tags --exact-match 2>/dev/null; then
        git describe --tags --exact-match
    elif git describe --tags 2>/dev/null; then
        git describe --tags | sed 's/-/./g'
    else
        echo "0.0.0-dev"
    fi
}

get_cpu_count() {
    if command -v nproc &> /dev/null; then
        nproc
    elif command -v sysctl &> /dev/null; then
        sysctl -n hw.ncpu
    else
        echo 4
    fi
}

# =============================================================================
# Parse Arguments
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# =============================================================================
# Pre-flight Checks
# =============================================================================

print_header "Grid-Watcher Release Build"

print_info "Checking dependencies..."
check_command "cmake"
check_command "git"

if [[ "$PLATFORM" == "linux" ]]; then
    check_command "g++"
elif [[ "$PLATFORM" == "windows" ]]; then
    check_command "cl.exe" || print_warning "MSVC not found in PATH"
fi

print_success "All dependencies found"

# Get version
VERSION=$(get_version)
print_info "Building version: $VERSION"
print_info "Platform: $PLATFORM"

# =============================================================================
# Clean Build (if requested)
# =============================================================================

if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning previous build..."
    rm -rf "$PROJECT_ROOT/$BUILD_DIR"
    rm -rf "$PROJECT_ROOT/$DIST_DIR"
    print_success "Clean complete"
fi

# =============================================================================
# Create Build Directory
# =============================================================================

print_info "Creating build directory..."
mkdir -p "$PROJECT_ROOT/$BUILD_DIR"
cd "$PROJECT_ROOT/$BUILD_DIR"

# =============================================================================
# Configure Build
# =============================================================================

print_header "Configuring Build"

CMAKE_ARGS=(
    "-DCMAKE_BUILD_TYPE=Release"
    "-DENABLE_LTO=ON"
    "-DENABLE_NATIVE=ON"
    "-DENABLE_PROFILING=OFF"
    "-DBUILD_BENCHMARKS=ON"
    "-DCMAKE_INSTALL_PREFIX=$PROJECT_ROOT/$DIST_DIR/$PROJECT_NAME-$VERSION"
)

if [[ "$PLATFORM" == "linux" ]]; then
    CMAKE_ARGS+=("-G" "Unix Makefiles")
elif [[ "$PLATFORM" == "windows" ]]; then
    CMAKE_ARGS+=("-G" "Visual Studio 17 2022")
fi

print_info "CMake configuration:"
for arg in "${CMAKE_ARGS[@]}"; do
    echo "  $arg"
done

cmake "${CMAKE_ARGS[@]}" ..

print_success "Configuration complete"

# =============================================================================
# Build
# =============================================================================

print_header "Building Project"

CPU_COUNT=$(get_cpu_count)
print_info "Building with $CPU_COUNT parallel jobs..."

if [[ "$PLATFORM" == "linux" ]]; then
    make -j"$CPU_COUNT"
elif [[ "$PLATFORM" == "windows" ]]; then
    cmake --build . --config Release --parallel "$CPU_COUNT"
fi

print_success "Build complete"

# =============================================================================
# Run Tests (if not skipped)
# =============================================================================

if [ "$SKIP_TESTS" = false ]; then
    print_header "Running Tests"
    
    if [ -f "./bin/test_grid_watcher" ]; then
        ./bin/test_grid_watcher
        print_success "Tests passed"
    else
        print_warning "No tests found, skipping..."
    fi
else
    print_warning "Tests skipped"
fi

# =============================================================================
# Install to Distribution Directory
# =============================================================================

print_header "Creating Distribution Package"

print_info "Installing files..."
cmake --install . --config Release

cd "$PROJECT_ROOT"

# Copy documentation
print_info "Copying documentation..."
cp README.md "$DIST_DIR/$PROJECT_NAME-$VERSION/"
cp CHANGELOG.md "$DIST_DIR/$PROJECT_NAME-$VERSION/"
cp LICENSE "$DIST_DIR/$PROJECT_NAME-$VERSION/"
cp -r docs/* "$DIST_DIR/$PROJECT_NAME-$VERSION/docs/" 2>/dev/null || true

# Create version file
print_info "Creating version file..."
cat > "$DIST_DIR/$PROJECT_NAME-$VERSION/VERSION" << EOF
Grid-Watcher $VERSION
Built: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
Platform: $PLATFORM
Compiler: $(cmake --version | head -n1)
Git Commit: $(git rev-parse HEAD 2>/dev/null || echo "unknown")
EOF

print_success "Installation complete"

# =============================================================================
# Create Archive
# =============================================================================

print_info "Creating release archive..."

cd "$DIST_DIR"

if [[ "$PLATFORM" == "linux" ]]; then
    ARCHIVE_NAME="$PROJECT_NAME-$VERSION-linux-$(uname -m).tar.gz"
    tar -czf "$ARCHIVE_NAME" "$PROJECT_NAME-$VERSION"
elif [[ "$PLATFORM" == "windows" ]]; then
    ARCHIVE_NAME="$PROJECT_NAME-$VERSION-windows-x64.zip"
    zip -r "$ARCHIVE_NAME" "$PROJECT_NAME-$VERSION"
fi

cd "$PROJECT_ROOT"

print_success "Archive created: $DIST_DIR/$ARCHIVE_NAME"

# =============================================================================
# Generate Checksums
# =============================================================================

print_info "Generating checksums..."

cd "$DIST_DIR"

if command -v sha256sum &> /dev/null; then
    sha256sum "$ARCHIVE_NAME" > "$ARCHIVE_NAME.sha256"
elif command -v shasum &> /dev/null; then
    shasum -a 256 "$ARCHIVE_NAME" > "$ARCHIVE_NAME.sha256"
fi

cd "$PROJECT_ROOT"

print_success "Checksum generated"

# =============================================================================
# Build Summary
# =============================================================================

print_header "Build Summary"

echo "Version:       $VERSION"
echo "Platform:      $PLATFORM"
echo "Archive:       $DIST_DIR/$ARCHIVE_NAME"
echo "Checksum:      $DIST_DIR/$ARCHIVE_NAME.sha256"
echo "Install Dir:   $DIST_DIR/$PROJECT_NAME-$VERSION"
echo ""

# Get binary size
if [ -f "$DIST_DIR/$PROJECT_NAME-$VERSION/bin/grid_watcher" ]; then
    BINARY_SIZE=$(du -h "$DIST_DIR/$PROJECT_NAME-$VERSION/bin/grid_watcher" | cut -f1)
    echo "Binary Size:   $BINARY_SIZE"
fi

# Get archive size
if [ -f "$DIST_DIR/$ARCHIVE_NAME" ]; then
    ARCHIVE_SIZE=$(du -h "$DIST_DIR/$ARCHIVE_NAME" | cut -f1)
    echo "Archive Size:  $ARCHIVE_SIZE"
fi

echo ""

# =============================================================================
# Next Steps
# =============================================================================

print_header "Next Steps"

echo "1. Test the release:"
echo "   cd $DIST_DIR/$PROJECT_NAME-$VERSION"
echo "   ./bin/grid_watcher"
echo ""
echo "2. Extract archive to test:"
echo "   cd $DIST_DIR"
echo "   tar -xzf $ARCHIVE_NAME  # Linux"
echo "   # or unzip $ARCHIVE_NAME  # Windows"
echo ""
echo "3. Upload to releases:"
echo "   gh release create v$VERSION $DIST_DIR/$ARCHIVE_NAME"
echo ""
echo "4. Update documentation:"
echo "   - Update release notes"
echo "   - Update changelog"
echo "   - Tag git repository"
echo ""

print_success "Build complete! 🎉"

# =============================================================================
# End of Script
# =============================================================================