# ./examples/albert/albert.py -*- Python -*-

# Copyright (C) 2022, Advanced Micro Devices, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

import torch
from torch.nn import functional as F
import torch_mlir

import air.mlir.ir
import air.mlir.passmanager
import air.compiler.util

import sys

M = 64
N = 64
K = 256
dtype=torch.bfloat16

class mmult(torch.nn.Module):
    def __init__(self):
        super().__init__()

    def forward(self, x, q, k, v):
        qk = torch.mm(torch.mm(x,q),torch.mm(x,k))
        qkv = torch.mm(F.softmax(qk,dim=-1), torch.mm(x,v))
        return qkv

program = mmult()
mlir = torch_mlir.compile(
    program,
    (torch.ones((M,K), dtype=dtype), torch.ones((K,N), dtype=dtype), 
    torch.ones((K,N), dtype=dtype), torch.ones((K,N), dtype=dtype)),
    output_type=torch_mlir.OutputType.LINALG_ON_TENSORS
)

args = sys.argv[1:]
if len(args) and args[0] == '-dump-linalg':
    print(mlir)
    exit(0)

with air.mlir.ir.Context():
    # convert torch_mlir.ir.Module to air.mlir.ir.Module
    air_module = air.mlir.ir.Module.parse(str(mlir))

    # convert linalg on tensors to linalg on memrefs
    pm = air.mlir.passmanager.PassManager.parse(
        air.compiler.util.LINALG_TENSOR_TO_MEMREF_PIPELINE)
    pm.run(air_module)

##  Removed from pipeline for now:
    pipeline = ",".join([
        "air-linalg-name",
        "air-linalg-codegen{input-filter=linalg.matmul1 herd-size=2,2 l1-tile-size=32,32,32}",
        "air-linalg-codegen{input-filter=linalg.matmul3 herd-size=2,2 l1-tile-size=32,32,32}",
        "air-linalg-codegen{input-filter=linalg.matmul5 herd-size=2,2 l1-tile-size=32,32,32}",
        "air-linalg-codegen{input-filter=linalg.generic8 herd-size=1,1 l1-tile-size=64,64,64}",
        "air-linalg-codegen{input-filter=linalg.generic9 herd-size=1,1 l1-tile-size=64,64,64}",
        "air-linalg-codegen{input-filter=linalg.generic10 herd-size=1,1 l1-tile-size=64,64,64}",
        "air-linalg-codegen{input-filter=linalg.generic12 herd-size=1,1 l1-tile-size=64,64,64}",
        "air-linalg-codegen{input-filter=linalg.generic13 herd-size=1,1 l1-tile-size=64,64,64}",
        "air-linalg-codegen{input-filter=linalg.matmul15 herd-size=2,2 l1-tile-size=32,32,32}",
        "air-linalg-codegen{input-filter=linalg.matmul17 herd-size=2,2 l1-tile-size=32,32,32}",
        "air-rm-linalg-name",
        #"canonicalize", "cse",
        "air-par-to-herd",
        "air-copy-to-dma",
        "air-par-to-launch{has-air-partition=true}",
        "canonicalize", "cse",
    ])
    pm = air.mlir.passmanager.PassManager.parse(pipeline)
    pm.run(air_module)

    with open('output1.mlir', 'w') as f:
        f.write(str(air_module))

    pipeline = ",".join([
        "air-dependency",
        "air-dependency-schedule-opt",
        "air-specialize-dma-broadcast",
    ])
    pm = air.mlir.passmanager.PassManager.parse(pipeline)
    pm.run(air_module)

    with open('output2.mlir', 'w') as f:
        f.write(str(air_module))
