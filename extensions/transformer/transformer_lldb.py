#!/usr/bin/env python3
# lldb -o "run" -o "command script import transformer.transformer_lldb" -o "continue" --batch -- python -B -m unittest -v transformer/transformer.py
import sys
import os
import atexit
import subprocess

import lldb

PROMPT="(lldb.py)"

def callback_e(frame, bp_loc, extra_args, internal_dict):
    current = frame
    while(current):
        print(PROMPT, current.name)
        current = current.parent
    
def callback_f(frame, bp_loc, internal_dict):
    callback_e(frame, bp_loc, None, internal_dict)

def __lldb_init_module(debugger, internal_dict):
    target = lldb.debugger.GetSelectedTarget()
    # bp = target.BreakpointCreateByRegex("setUpObjCRuntimeGetImageNameFromClass")
    # bp.SetScriptCallbackFunction(f"{__name__}.callback_f")
    # bp.SetAutoContinue(True)
    
    bp = target.BreakpointCreateByRegex("_transformer_encoder_layer_fwd\(")
    bp.SetScriptCallbackFunction(f"{__name__}.callback_f")
    bp.SetAutoContinue(True)
    
    print(PROMPT, "__lldb_init_module")
