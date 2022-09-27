//===- AIRTilingUtils.h -----------------------------------------*- C++ -*-===//
//
// Copyright (C) 2021-2022, Xilinx Inc.
// Copyright (C) 2022, Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
//===----------------------------------------------------------------------===//

//===- AIRTilingUtils.h - AIR Loop tiling utilities ------------------------===//
//
// This header file defines utility functions that are commonly used in passes,
// primarily AIR automatic loop tiling passes.
//===-----------------------------------------------------------------------===//

#pragma once

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"

using namespace mlir;

namespace xilinx {
namespace air {
/// Identify valid and profitable bands of loops to tile. This is currently just
/// a temporary placeholder to test the mechanics of tiled code generation.
/// Returns all maximal outermost perfect loop nests that has been attached with
/// the given label to tile.
void getTileableBands(func::FuncOp f,
                      std::vector<SmallVector<AffineForOp, 6>> &bands,
                      const char *attrName, StringRef label);

/// Get the loop band that has been attached with the given label.
AffineForOp getLabel(AffineForOp root, StringRef label, const char* attrName);

} // namespace air
} // namespace xilinx
