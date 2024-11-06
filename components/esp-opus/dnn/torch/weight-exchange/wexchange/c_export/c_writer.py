"""
/* Copyright (c) 2023 Amazon
   Written by Jan Buethe */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
"""

import os
from collections import OrderedDict

class CWriter:
    def __init__(self,
                 filename_without_extension,
                 message=None,
                 header_only=False,
                 create_state_struct=False,
                 enable_binary_blob=True,
                 model_struct_name="Model",
                 nnet_header="nnet.h",
                 add_typedef=False):
        """
        Writer class for creating souce and header files for weight exports to C

        Parameters:
        -----------

        filename_without_extension: str
            filename from which .c and .h files are created

        message: str, optional
            if given and not None, this message will be printed as comment in the header file

        header_only: bool, optional
            if True, only a header file is created; defaults to False

        enable_binary_blob: bool, optional
            if True, export is done in binary blob format and a model type is created; defaults to False

        create_state_struct: bool, optional
            if True, a state struct type is created in the header file; if False, state sizes are defined as macros; defaults to False

        model_struct_name: str, optional
            name used for the model struct type; only relevant when enable_binary_blob is True; defaults to "Model"

        nnet_header: str, optional
            name of header nnet header file; defaults to nnet.h

        """


        self.header_only = header_only
        self.enable_binary_blob = enable_binary_blob
        self.create_state_struct = create_state_struct
        self.model_struct_name = model_struct_name
        self.add_typedef = add_typedef

        # for binary blob format, format is key=<layer name>, value=(<layer type>, <init call>)
        self.layer_dict = OrderedDict()

        # for binary blob format, format is key=<layer name>, value=<layer type>
        self.weight_arrays = []

        # form model struct, format is key=<layer name>, value=<number of elements>
        self.state_dict = OrderedDict()

        self.header = open(filename_without_extension + ".h", "w")
        header_name = os.path.basename(filename_without_extension) + '.h'

        if message is not None:
            self.header.write(f"/* {message} */\n\n")

        self.header_guard = os.path.basename(filename_without_extension).upper() + "_H"
        self.header.write(
f'''
#ifndef {self.header_guard}
#define {self.header_guard}

#include "{nnet_header}"

'''
        )

        if not self.header_only:
            self.source = open(filename_without_extension + ".c", "w")
            if message is not None:
                self.source.write(f"/* {message} */\n\n")

            self.source.write(
f"""
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

""")
            self.source.write(f'#include "{header_name}"\n\n')


    def _finalize_header(self):

        # create model type
        if self.add_typedef:
            self.header.write(f"\ntypedef struct {{")
        else:
            self.header.write(f"\nstruct {self.model_struct_name} {{")
        for name, data in self.layer_dict.items():
            layer_type = data[0]
            self.header.write(f"\n    {layer_type} {name};")
        if self.add_typedef:
            self.header.write(f"\n}} {self.model_struct_name};\n")
        else:
            self.header.write(f"\n}};\n")

        init_prototype = f"int init_{self.model_struct_name.lower()}({self.model_struct_name} *model, const WeightArray *arrays)"
        self.header.write(f"\n{init_prototype};\n")

        self.header.write(f"\n#endif /* {self.header_guard} */\n")

    def _finalize_source(self):


        # create weight array
        if len(set(self.weight_arrays)) != len(self.weight_arrays):
            raise ValueError("error: detected duplicates in weight arrays")
        if self.enable_binary_blob: self.source.write("\n#ifndef USE_WEIGHTS_FILE\n")
        self.source.write(f"const WeightArray {self.model_struct_name.lower()}_arrays[] = {{\n")
        for name in self.weight_arrays:
            self.source.write(f"#ifdef WEIGHTS_{name}_DEFINED\n")
            self.source.write(f'    {{"{name}",  WEIGHTS_{name}_TYPE, sizeof({name}), {name}}},\n')
            self.source.write(f"#endif\n")
        self.source.write("    {NULL, 0, 0, NULL}\n")
        self.source.write("};\n")

        if self.enable_binary_blob: self.source.write("#endif /* USE_WEIGHTS_FILE */\n")

        # create init function definition
        init_prototype = f"int init_{self.model_struct_name.lower()}({self.model_struct_name} *model, const WeightArray *arrays)"
        if self.enable_binary_blob: self.source.write("\n#ifndef DUMP_BINARY_WEIGHTS\n")
        self.source.write(f"{init_prototype} {{\n")
        for name, data in self.layer_dict.items():
            self.source.write(f"    if ({data[1]}) return 1;\n")
        self.source.write("    return 0;\n")
        self.source.write("}\n")
        if self.enable_binary_blob:self.source.write("#endif /* DUMP_BINARY_WEIGHTS */\n")


    def close(self):

        if not self.header_only:
            self._finalize_source()
            self.source.close()

        self._finalize_header()
        self.header.close()

    def __del__(self):
        try:
            self.close()
        except:
            pass