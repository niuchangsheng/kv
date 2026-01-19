#!/bin/bash

# KV Engine Coverage Report Generation Script
# This script generates code coverage reports using gcov and lcov

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
COVERAGE_DIR="${BUILD_DIR}/coverage"
COVERAGE_INFO="${BUILD_DIR}/coverage.info"
COVERAGE_FILTERED="${BUILD_DIR}/coverage_filtered.info"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}KV Engine Coverage Report Generator${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if required tools are installed
echo -e "${YELLOW}Checking dependencies...${NC}"
if ! command -v gcov &> /dev/null; then
    echo -e "${RED}Error: gcov is not installed${NC}"
    echo "Install with: sudo apt install gcov (Ubuntu/Debian) or brew install gcov (macOS)"
    exit 1
fi

if ! command -v lcov &> /dev/null; then
    echo -e "${RED}Error: lcov is not installed${NC}"
    echo "Install with: sudo apt install lcov (Ubuntu/Debian) or brew install lcov (macOS)"
    exit 1
fi

if ! command -v genhtml &> /dev/null; then
    echo -e "${RED}Error: genhtml is not installed (part of lcov)${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All dependencies found${NC}"
echo ""

# Clean previous build
echo -e "${YELLOW}Cleaning previous build...${NC}"
if [ -d "${BUILD_DIR}" ]; then
    rm -rf "${BUILD_DIR}"
fi
mkdir -p "${BUILD_DIR}"
echo -e "${GREEN}✓ Cleaned${NC}"
echo ""

# Configure and build with coverage
echo -e "${YELLOW}Configuring CMake with coverage flags...${NC}"
cd "${BUILD_DIR}"
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage -g -O0"

echo -e "${GREEN}✓ Configured${NC}"
echo ""

# Build project
echo -e "${YELLOW}Building project...${NC}"
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo -e "${GREEN}✓ Built${NC}"
echo ""

# Run tests
echo -e "${YELLOW}Running tests...${NC}"
if [ -f "${BUILD_DIR}/bin/test_kv_engine" ]; then
    "${BUILD_DIR}/bin/test_kv_engine"
    echo -e "${GREEN}✓ Tests passed${NC}"
else
    echo -e "${RED}Error: test_kv_engine not found${NC}"
    exit 1
fi
echo ""

# Generate coverage data
echo -e "${YELLOW}Generating coverage data...${NC}"
# First capture all coverage data (including external)
lcov --capture \
     --directory "${BUILD_DIR}" \
     --output-file "${COVERAGE_INFO}" \
     --ignore-errors source,mismatch

# Check if coverage data was generated
if [ ! -f "${COVERAGE_INFO}" ]; then
    echo -e "${RED}Error: Failed to generate coverage.info${NC}"
    exit 1
fi

# Check if coverage data has valid records
if ! lcov --summary "${COVERAGE_INFO}" &>/dev/null; then
    echo -e "${RED}Error: No valid coverage data found${NC}"
    echo -e "${YELLOW}Hint: Make sure tests were run and .gcda files exist${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Coverage data generated${NC}"
echo ""

# Filter coverage data (exclude test directory and system libraries)
echo -e "${YELLOW}Filtering coverage data...${NC}"
lcov --remove "${COVERAGE_INFO}" \
     '/usr/*' \
     '*/test/*' \
     '*/build/*' \
     '*/CMakeFiles/*' \
     '*/13/*' \
     '*/c++/*' \
     --output-file "${COVERAGE_FILTERED}" \
     --ignore-errors unused,empty

# Check if filtered data is valid
if [ ! -f "${COVERAGE_FILTERED}" ]; then
    echo -e "${RED}Error: Failed to filter coverage data${NC}"
    exit 1
fi

# Verify filtered data has content
if ! lcov --summary "${COVERAGE_FILTERED}" &>/dev/null; then
    echo -e "${YELLOW}Warning: Filtered coverage data is empty, using original data${NC}"
    cp "${COVERAGE_INFO}" "${COVERAGE_FILTERED}"
fi

echo -e "${GREEN}✓ Coverage data filtered${NC}"
echo ""

# Generate HTML report
echo -e "${YELLOW}Generating HTML report...${NC}"
genhtml "${COVERAGE_FILTERED}" \
        --output-directory "${COVERAGE_DIR}" \
        --title "KV Engine Coverage Report (src only)" \
        --show-details \
        --legend \
        --demangle-cpp

if [ ! -d "${COVERAGE_DIR}" ]; then
    echo -e "${RED}Error: Failed to generate HTML report${NC}"
    exit 1
fi

echo -e "${GREEN}✓ HTML report generated${NC}"
echo ""

# Display summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Coverage Summary${NC}"
echo -e "${BLUE}========================================${NC}"
lcov --summary "${COVERAGE_FILTERED}" 2>&1 | grep -E "(Summary|lines|functions|branches)" || true
echo ""

# Display file-level coverage
echo -e "${BLUE}File-level Coverage:${NC}"
lcov --list "${COVERAGE_FILTERED}" 2>&1 | grep "src/" | head -10 || true
echo ""

# Report location
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Coverage report generated successfully!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Report location: ${GREEN}${COVERAGE_DIR}/index.html${NC}"
echo ""
echo -e "To view the report:"
echo -e "  ${YELLOW}Linux:${NC}   xdg-open ${COVERAGE_DIR}/index.html"
echo -e "  ${YELLOW}macOS:${NC}   open ${COVERAGE_DIR}/index.html"
echo -e "  ${YELLOW}Windows:${NC} start ${COVERAGE_DIR}/index.html"
echo ""

# Optional: Open report automatically
if [ "$1" == "--open" ] || [ "$1" == "-o" ]; then
    if command -v xdg-open &> /dev/null; then
        xdg-open "${COVERAGE_DIR}/index.html"
    elif command -v open &> /dev/null; then
        open "${COVERAGE_DIR}/index.html"
    else
        echo -e "${YELLOW}Note: Could not auto-open report. Please open manually.${NC}"
    fi
fi

exit 0
