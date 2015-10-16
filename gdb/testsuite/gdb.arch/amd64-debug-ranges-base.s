/* This testcase is part of GDB, the GNU debugger.

   Copyright 2015 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* This file is compiled from gdb.arch/amd64-debug-ranges-base.c
   using '-g3 -O0 -S', after which the .debug_ranges sections was
   re-written by hand in order to contain the required (valid) DWARF.  */

        .file	"amd64-debug-ranges-base.c"
	.text
.Ltext0:
	.section	.text.3,"ax",@progbits
	.globl	frame3
	.type	frame3, @function
frame3:
.LFB0:
	.file 1 "amd64-debug-ranges-base.c"
	.loc 1 20 0
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	.loc 1 21 0
	movl	$0, %eax
	.loc 1 22 0
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	frame3, .-frame3
	.section	.text.2,"ax",@progbits
	.globl	frame2
	.type	frame2, @function
frame2:
.LFB1:
	.loc 1 26 0
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	.loc 1 27 0
	movl	$0, %eax
	.loc 1 28 0
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	frame2, .-frame2
	.section	.text.1,"ax",@progbits
	.globl	main
	.type	main, @function
main:
.LFB2:
	.loc 1 32 0
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	.loc 1 33 0
	call	frame2
	.loc 1 34 0
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE2:
	.size	main, .-main
	.text
.Letext0:
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x88
	.value	0x4
	.long	.Ldebug_abbrev0
	.byte	0x8
	.uleb128 0x1
	.long	.LASF251
	.byte	0xc
	.long	.LASF252
	.long	.LASF253
	.long	.Ldebug_ranges0+0
	.quad	0
	.long	.Ldebug_line0
	.long	.Ldebug_macro0
	.uleb128 0x2
	.long	.LASF248
	.byte	0x1
	.byte	0x13
	.long	0x4a
	.quad	.LFB0
	.quad	.LFE0-.LFB0
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0x3
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x2
	.long	.LASF249
	.byte	0x1
	.byte	0x19
	.long	0x4a
	.quad	.LFB1
	.quad	.LFE1-.LFB1
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0x4
	.long	.LASF250
	.byte	0x1
	.byte	0x1f
	.long	0x4a
	.quad	.LFB2
	.quad	.LFE2-.LFB2
	.uleb128 0x1
	.byte	0x9c
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x55
	.uleb128 0x17
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x17
	.uleb128 0x2119
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2117
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2116
	.uleb128 0x19
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_aranges,"",@progbits
	.long	0x4c
	.value	0x2
	.long	.Ldebug_info0
	.byte	0x8
	.byte	0
	.value	0
	.value	0
	.quad	.LFB0
	.quad	.LFE0-.LFB0
	.quad	.LFB1
	.quad	.LFE1-.LFB1
	.quad	.LFB2
	.quad	.LFE2-.LFB2
	.quad	0
	.quad	0
	.section	.debug_ranges,"",@progbits
.Ldebug_ranges0:
	.quad	0xffffffffffffffff
	.quad	.LFB0
	.quad	0
	.quad	.LFE0 - .LFB0
	.quad	0xffffffffffffffff
	.quad	.LFB1
	.quad	0
	.quad	.LFE1 - .LFB1
	.quad	0xffffffffffffffff
	.quad	.LFB2
	.quad	0
	.quad	.LFE2 - .LFB2
	.quad	0
	.quad	0
	.section	.debug_macro,"",@progbits
.Ldebug_macro0:
	.value	0x4
	.byte	0x2
	.long	.Ldebug_line0
	.byte	0x7
	.long	.Ldebug_macro1
	.byte	0x3
	.uleb128 0
	.uleb128 0x1
	.file 2 "/usr/include/stdc-predef.h"
	.byte	0x3
	.uleb128 0
	.uleb128 0x2
	.byte	0x7
	.long	.Ldebug_macro2
	.byte	0x4
	.byte	0x4
	.byte	0
	.section	.debug_macro,"G",@progbits,wm4.0.9fc6967513006a990da6a9952cf266ea,comdat
.Ldebug_macro1:
	.value	0x4
	.byte	0
	.byte	0x5
	.uleb128 0
	.long	.LASF0
	.byte	0x5
	.uleb128 0
	.long	.LASF1
	.byte	0x5
	.uleb128 0
	.long	.LASF2
	.byte	0x5
	.uleb128 0
	.long	.LASF3
	.byte	0x5
	.uleb128 0
	.long	.LASF4
	.byte	0x5
	.uleb128 0
	.long	.LASF5
	.byte	0x5
	.uleb128 0
	.long	.LASF6
	.byte	0x5
	.uleb128 0
	.long	.LASF7
	.byte	0x5
	.uleb128 0
	.long	.LASF8
	.byte	0x5
	.uleb128 0
	.long	.LASF9
	.byte	0x5
	.uleb128 0
	.long	.LASF10
	.byte	0x5
	.uleb128 0
	.long	.LASF11
	.byte	0x5
	.uleb128 0
	.long	.LASF12
	.byte	0x5
	.uleb128 0
	.long	.LASF13
	.byte	0x5
	.uleb128 0
	.long	.LASF14
	.byte	0x5
	.uleb128 0
	.long	.LASF15
	.byte	0x5
	.uleb128 0
	.long	.LASF16
	.byte	0x5
	.uleb128 0
	.long	.LASF17
	.byte	0x5
	.uleb128 0
	.long	.LASF18
	.byte	0x5
	.uleb128 0
	.long	.LASF19
	.byte	0x5
	.uleb128 0
	.long	.LASF20
	.byte	0x5
	.uleb128 0
	.long	.LASF21
	.byte	0x5
	.uleb128 0
	.long	.LASF22
	.byte	0x5
	.uleb128 0
	.long	.LASF23
	.byte	0x5
	.uleb128 0
	.long	.LASF24
	.byte	0x5
	.uleb128 0
	.long	.LASF25
	.byte	0x5
	.uleb128 0
	.long	.LASF26
	.byte	0x5
	.uleb128 0
	.long	.LASF27
	.byte	0x5
	.uleb128 0
	.long	.LASF28
	.byte	0x5
	.uleb128 0
	.long	.LASF29
	.byte	0x5
	.uleb128 0
	.long	.LASF30
	.byte	0x5
	.uleb128 0
	.long	.LASF31
	.byte	0x5
	.uleb128 0
	.long	.LASF32
	.byte	0x5
	.uleb128 0
	.long	.LASF33
	.byte	0x5
	.uleb128 0
	.long	.LASF34
	.byte	0x5
	.uleb128 0
	.long	.LASF35
	.byte	0x5
	.uleb128 0
	.long	.LASF36
	.byte	0x5
	.uleb128 0
	.long	.LASF37
	.byte	0x5
	.uleb128 0
	.long	.LASF38
	.byte	0x5
	.uleb128 0
	.long	.LASF39
	.byte	0x5
	.uleb128 0
	.long	.LASF40
	.byte	0x5
	.uleb128 0
	.long	.LASF41
	.byte	0x5
	.uleb128 0
	.long	.LASF42
	.byte	0x5
	.uleb128 0
	.long	.LASF43
	.byte	0x5
	.uleb128 0
	.long	.LASF44
	.byte	0x5
	.uleb128 0
	.long	.LASF45
	.byte	0x5
	.uleb128 0
	.long	.LASF46
	.byte	0x5
	.uleb128 0
	.long	.LASF47
	.byte	0x5
	.uleb128 0
	.long	.LASF48
	.byte	0x5
	.uleb128 0
	.long	.LASF49
	.byte	0x5
	.uleb128 0
	.long	.LASF50
	.byte	0x5
	.uleb128 0
	.long	.LASF51
	.byte	0x5
	.uleb128 0
	.long	.LASF52
	.byte	0x5
	.uleb128 0
	.long	.LASF53
	.byte	0x5
	.uleb128 0
	.long	.LASF54
	.byte	0x5
	.uleb128 0
	.long	.LASF55
	.byte	0x5
	.uleb128 0
	.long	.LASF56
	.byte	0x5
	.uleb128 0
	.long	.LASF57
	.byte	0x5
	.uleb128 0
	.long	.LASF58
	.byte	0x5
	.uleb128 0
	.long	.LASF59
	.byte	0x5
	.uleb128 0
	.long	.LASF60
	.byte	0x5
	.uleb128 0
	.long	.LASF61
	.byte	0x5
	.uleb128 0
	.long	.LASF62
	.byte	0x5
	.uleb128 0
	.long	.LASF63
	.byte	0x5
	.uleb128 0
	.long	.LASF64
	.byte	0x5
	.uleb128 0
	.long	.LASF65
	.byte	0x5
	.uleb128 0
	.long	.LASF66
	.byte	0x5
	.uleb128 0
	.long	.LASF67
	.byte	0x5
	.uleb128 0
	.long	.LASF68
	.byte	0x5
	.uleb128 0
	.long	.LASF69
	.byte	0x5
	.uleb128 0
	.long	.LASF70
	.byte	0x5
	.uleb128 0
	.long	.LASF71
	.byte	0x5
	.uleb128 0
	.long	.LASF72
	.byte	0x5
	.uleb128 0
	.long	.LASF73
	.byte	0x5
	.uleb128 0
	.long	.LASF74
	.byte	0x5
	.uleb128 0
	.long	.LASF75
	.byte	0x5
	.uleb128 0
	.long	.LASF76
	.byte	0x5
	.uleb128 0
	.long	.LASF77
	.byte	0x5
	.uleb128 0
	.long	.LASF78
	.byte	0x5
	.uleb128 0
	.long	.LASF79
	.byte	0x5
	.uleb128 0
	.long	.LASF80
	.byte	0x5
	.uleb128 0
	.long	.LASF81
	.byte	0x5
	.uleb128 0
	.long	.LASF82
	.byte	0x5
	.uleb128 0
	.long	.LASF83
	.byte	0x5
	.uleb128 0
	.long	.LASF84
	.byte	0x5
	.uleb128 0
	.long	.LASF85
	.byte	0x5
	.uleb128 0
	.long	.LASF86
	.byte	0x5
	.uleb128 0
	.long	.LASF87
	.byte	0x5
	.uleb128 0
	.long	.LASF88
	.byte	0x5
	.uleb128 0
	.long	.LASF89
	.byte	0x5
	.uleb128 0
	.long	.LASF90
	.byte	0x5
	.uleb128 0
	.long	.LASF91
	.byte	0x5
	.uleb128 0
	.long	.LASF92
	.byte	0x5
	.uleb128 0
	.long	.LASF93
	.byte	0x5
	.uleb128 0
	.long	.LASF94
	.byte	0x5
	.uleb128 0
	.long	.LASF95
	.byte	0x5
	.uleb128 0
	.long	.LASF96
	.byte	0x5
	.uleb128 0
	.long	.LASF97
	.byte	0x5
	.uleb128 0
	.long	.LASF98
	.byte	0x5
	.uleb128 0
	.long	.LASF99
	.byte	0x5
	.uleb128 0
	.long	.LASF100
	.byte	0x5
	.uleb128 0
	.long	.LASF101
	.byte	0x5
	.uleb128 0
	.long	.LASF102
	.byte	0x5
	.uleb128 0
	.long	.LASF103
	.byte	0x5
	.uleb128 0
	.long	.LASF104
	.byte	0x5
	.uleb128 0
	.long	.LASF105
	.byte	0x5
	.uleb128 0
	.long	.LASF106
	.byte	0x5
	.uleb128 0
	.long	.LASF107
	.byte	0x5
	.uleb128 0
	.long	.LASF108
	.byte	0x5
	.uleb128 0
	.long	.LASF109
	.byte	0x5
	.uleb128 0
	.long	.LASF110
	.byte	0x5
	.uleb128 0
	.long	.LASF111
	.byte	0x5
	.uleb128 0
	.long	.LASF112
	.byte	0x5
	.uleb128 0
	.long	.LASF113
	.byte	0x5
	.uleb128 0
	.long	.LASF114
	.byte	0x5
	.uleb128 0
	.long	.LASF115
	.byte	0x5
	.uleb128 0
	.long	.LASF116
	.byte	0x5
	.uleb128 0
	.long	.LASF117
	.byte	0x5
	.uleb128 0
	.long	.LASF118
	.byte	0x5
	.uleb128 0
	.long	.LASF119
	.byte	0x5
	.uleb128 0
	.long	.LASF120
	.byte	0x5
	.uleb128 0
	.long	.LASF121
	.byte	0x5
	.uleb128 0
	.long	.LASF122
	.byte	0x5
	.uleb128 0
	.long	.LASF123
	.byte	0x5
	.uleb128 0
	.long	.LASF124
	.byte	0x5
	.uleb128 0
	.long	.LASF125
	.byte	0x5
	.uleb128 0
	.long	.LASF126
	.byte	0x5
	.uleb128 0
	.long	.LASF127
	.byte	0x5
	.uleb128 0
	.long	.LASF128
	.byte	0x5
	.uleb128 0
	.long	.LASF129
	.byte	0x5
	.uleb128 0
	.long	.LASF130
	.byte	0x5
	.uleb128 0
	.long	.LASF131
	.byte	0x5
	.uleb128 0
	.long	.LASF132
	.byte	0x5
	.uleb128 0
	.long	.LASF133
	.byte	0x5
	.uleb128 0
	.long	.LASF134
	.byte	0x5
	.uleb128 0
	.long	.LASF135
	.byte	0x5
	.uleb128 0
	.long	.LASF136
	.byte	0x5
	.uleb128 0
	.long	.LASF137
	.byte	0x5
	.uleb128 0
	.long	.LASF138
	.byte	0x5
	.uleb128 0
	.long	.LASF139
	.byte	0x5
	.uleb128 0
	.long	.LASF140
	.byte	0x5
	.uleb128 0
	.long	.LASF141
	.byte	0x5
	.uleb128 0
	.long	.LASF142
	.byte	0x5
	.uleb128 0
	.long	.LASF143
	.byte	0x5
	.uleb128 0
	.long	.LASF144
	.byte	0x5
	.uleb128 0
	.long	.LASF145
	.byte	0x5
	.uleb128 0
	.long	.LASF146
	.byte	0x5
	.uleb128 0
	.long	.LASF147
	.byte	0x5
	.uleb128 0
	.long	.LASF148
	.byte	0x5
	.uleb128 0
	.long	.LASF149
	.byte	0x5
	.uleb128 0
	.long	.LASF150
	.byte	0x5
	.uleb128 0
	.long	.LASF151
	.byte	0x5
	.uleb128 0
	.long	.LASF152
	.byte	0x5
	.uleb128 0
	.long	.LASF153
	.byte	0x5
	.uleb128 0
	.long	.LASF154
	.byte	0x5
	.uleb128 0
	.long	.LASF155
	.byte	0x5
	.uleb128 0
	.long	.LASF156
	.byte	0x5
	.uleb128 0
	.long	.LASF157
	.byte	0x5
	.uleb128 0
	.long	.LASF158
	.byte	0x5
	.uleb128 0
	.long	.LASF159
	.byte	0x5
	.uleb128 0
	.long	.LASF160
	.byte	0x5
	.uleb128 0
	.long	.LASF161
	.byte	0x5
	.uleb128 0
	.long	.LASF162
	.byte	0x5
	.uleb128 0
	.long	.LASF163
	.byte	0x5
	.uleb128 0
	.long	.LASF164
	.byte	0x5
	.uleb128 0
	.long	.LASF165
	.byte	0x5
	.uleb128 0
	.long	.LASF166
	.byte	0x5
	.uleb128 0
	.long	.LASF167
	.byte	0x5
	.uleb128 0
	.long	.LASF168
	.byte	0x5
	.uleb128 0
	.long	.LASF169
	.byte	0x5
	.uleb128 0
	.long	.LASF170
	.byte	0x5
	.uleb128 0
	.long	.LASF171
	.byte	0x5
	.uleb128 0
	.long	.LASF172
	.byte	0x5
	.uleb128 0
	.long	.LASF173
	.byte	0x5
	.uleb128 0
	.long	.LASF174
	.byte	0x5
	.uleb128 0
	.long	.LASF175
	.byte	0x5
	.uleb128 0
	.long	.LASF176
	.byte	0x5
	.uleb128 0
	.long	.LASF177
	.byte	0x5
	.uleb128 0
	.long	.LASF178
	.byte	0x5
	.uleb128 0
	.long	.LASF179
	.byte	0x5
	.uleb128 0
	.long	.LASF180
	.byte	0x5
	.uleb128 0
	.long	.LASF181
	.byte	0x5
	.uleb128 0
	.long	.LASF182
	.byte	0x5
	.uleb128 0
	.long	.LASF183
	.byte	0x5
	.uleb128 0
	.long	.LASF184
	.byte	0x5
	.uleb128 0
	.long	.LASF185
	.byte	0x5
	.uleb128 0
	.long	.LASF186
	.byte	0x5
	.uleb128 0
	.long	.LASF187
	.byte	0x5
	.uleb128 0
	.long	.LASF188
	.byte	0x5
	.uleb128 0
	.long	.LASF189
	.byte	0x5
	.uleb128 0
	.long	.LASF190
	.byte	0x5
	.uleb128 0
	.long	.LASF191
	.byte	0x5
	.uleb128 0
	.long	.LASF192
	.byte	0x5
	.uleb128 0
	.long	.LASF193
	.byte	0x5
	.uleb128 0
	.long	.LASF194
	.byte	0x5
	.uleb128 0
	.long	.LASF195
	.byte	0x5
	.uleb128 0
	.long	.LASF196
	.byte	0x5
	.uleb128 0
	.long	.LASF197
	.byte	0x5
	.uleb128 0
	.long	.LASF198
	.byte	0x5
	.uleb128 0
	.long	.LASF199
	.byte	0x5
	.uleb128 0
	.long	.LASF200
	.byte	0x5
	.uleb128 0
	.long	.LASF201
	.byte	0x5
	.uleb128 0
	.long	.LASF202
	.byte	0x5
	.uleb128 0
	.long	.LASF203
	.byte	0x5
	.uleb128 0
	.long	.LASF204
	.byte	0x5
	.uleb128 0
	.long	.LASF205
	.byte	0x5
	.uleb128 0
	.long	.LASF206
	.byte	0x5
	.uleb128 0
	.long	.LASF207
	.byte	0x5
	.uleb128 0
	.long	.LASF208
	.byte	0x5
	.uleb128 0
	.long	.LASF209
	.byte	0x5
	.uleb128 0
	.long	.LASF210
	.byte	0x5
	.uleb128 0
	.long	.LASF211
	.byte	0x5
	.uleb128 0
	.long	.LASF212
	.byte	0x5
	.uleb128 0
	.long	.LASF213
	.byte	0x5
	.uleb128 0
	.long	.LASF214
	.byte	0x5
	.uleb128 0
	.long	.LASF215
	.byte	0x5
	.uleb128 0
	.long	.LASF216
	.byte	0x5
	.uleb128 0
	.long	.LASF217
	.byte	0x5
	.uleb128 0
	.long	.LASF218
	.byte	0x5
	.uleb128 0
	.long	.LASF219
	.byte	0x5
	.uleb128 0
	.long	.LASF220
	.byte	0x5
	.uleb128 0
	.long	.LASF221
	.byte	0x5
	.uleb128 0
	.long	.LASF222
	.byte	0x5
	.uleb128 0
	.long	.LASF223
	.byte	0x5
	.uleb128 0
	.long	.LASF224
	.byte	0x5
	.uleb128 0
	.long	.LASF225
	.byte	0x5
	.uleb128 0
	.long	.LASF226
	.byte	0x5
	.uleb128 0
	.long	.LASF227
	.byte	0x5
	.uleb128 0
	.long	.LASF228
	.byte	0x5
	.uleb128 0
	.long	.LASF229
	.byte	0x5
	.uleb128 0
	.long	.LASF230
	.byte	0x5
	.uleb128 0
	.long	.LASF231
	.byte	0x5
	.uleb128 0
	.long	.LASF232
	.byte	0x5
	.uleb128 0
	.long	.LASF233
	.byte	0x5
	.uleb128 0
	.long	.LASF234
	.byte	0x5
	.uleb128 0
	.long	.LASF235
	.byte	0x5
	.uleb128 0
	.long	.LASF236
	.byte	0x5
	.uleb128 0
	.long	.LASF237
	.byte	0x5
	.uleb128 0
	.long	.LASF238
	.byte	0x5
	.uleb128 0
	.long	.LASF239
	.byte	0x5
	.uleb128 0
	.long	.LASF240
	.byte	0x5
	.uleb128 0
	.long	.LASF241
	.byte	0x5
	.uleb128 0
	.long	.LASF242
	.byte	0
	.section	.debug_macro,"G",@progbits,wm4.stdcpredef.h.19.52894a18705f0664f8da17e99c6c0a93,comdat
.Ldebug_macro2:
	.value	0x4
	.byte	0
	.byte	0x5
	.uleb128 0x13
	.long	.LASF243
	.byte	0x5
	.uleb128 0x26
	.long	.LASF244
	.byte	0x5
	.uleb128 0x2e
	.long	.LASF245
	.byte	0x5
	.uleb128 0x3b
	.long	.LASF246
	.byte	0x5
	.uleb128 0x3e
	.long	.LASF247
	.byte	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF106:
	.string	"__UINT_LEAST8_MAX__ 0xff"
.LASF153:
	.string	"__DBL_DENORM_MIN__ ((double)4.94065645841246544177e-324L)"
.LASF218:
	.string	"__amd64__ 1"
.LASF235:
	.string	"__linux 1"
.LASF110:
	.string	"__UINT_LEAST32_MAX__ 0xffffffffU"
.LASF26:
	.string	"__SIZEOF_SIZE_T__ 8"
.LASF8:
	.string	"__VERSION__ \"5.1.1 20150618 (Red Hat 5.1.1-4)\""
.LASF1:
	.string	"__STDC_VERSION__ 201112L"
.LASF34:
	.string	"__SIZEOF_POINTER__ 8"
.LASF252:
	.string	"amd64-debug-ranges-base.c"
.LASF178:
	.string	"__DEC64_MANT_DIG__ 16"
.LASF128:
	.string	"__FLT_RADIX__ 2"
.LASF165:
	.string	"__LDBL_MIN__ 3.36210314311209350626e-4932L"
.LASF239:
	.string	"__unix__ 1"
.LASF133:
	.string	"__FLT_MAX_EXP__ 128"
.LASF162:
	.string	"__LDBL_MAX_10_EXP__ 4932"
.LASF215:
	.string	"__SIZEOF_WINT_T__ 4"
.LASF148:
	.string	"__DBL_MAX_10_EXP__ 308"
.LASF121:
	.string	"__UINT_FAST64_MAX__ 0xffffffffffffffffUL"
.LASF217:
	.string	"__amd64 1"
.LASF38:
	.string	"__WINT_TYPE__ unsigned int"
.LASF113:
	.string	"__UINT64_C(c) c ## UL"
.LASF155:
	.string	"__DBL_HAS_INFINITY__ 1"
.LASF233:
	.string	"__SSE2_MATH__ 1"
.LASF7:
	.string	"__GNUC_PATCHLEVEL__ 1"
.LASF236:
	.string	"__linux__ 1"
.LASF4:
	.string	"__STDC_HOSTED__ 1"
.LASF81:
	.string	"__WINT_MIN__ 0U"
.LASF222:
	.string	"__SIZEOF_FLOAT128__ 16"
.LASF246:
	.string	"__STDC_ISO_10646__ 201304L"
.LASF50:
	.string	"__UINT32_TYPE__ unsigned int"
.LASF231:
	.string	"__FXSR__ 1"
.LASF184:
	.string	"__DEC64_SUBNORMAL_MIN__ 0.000000000000001E-383DD"
.LASF136:
	.string	"__FLT_MAX__ 3.40282346638528859812e+38F"
.LASF253:
	.string	"/path/to/binutils-gdb/src/gdb/testsuite/gdb.arch"
.LASF237:
	.string	"linux 1"
.LASF114:
	.string	"__INT_FAST8_MAX__ 0x7f"
.LASF156:
	.string	"__DBL_HAS_QUIET_NAN__ 1"
.LASF143:
	.string	"__DBL_MANT_DIG__ 53"
.LASF48:
	.string	"__UINT8_TYPE__ unsigned char"
.LASF173:
	.string	"__DEC32_MAX_EXP__ 97"
.LASF46:
	.string	"__INT32_TYPE__ int"
.LASF43:
	.string	"__SIG_ATOMIC_TYPE__ int"
.LASF145:
	.string	"__DBL_MIN_EXP__ (-1021)"
.LASF169:
	.string	"__LDBL_HAS_INFINITY__ 1"
.LASF20:
	.string	"__SIZEOF_LONG__ 8"
.LASF140:
	.string	"__FLT_HAS_DENORM__ 1"
.LASF95:
	.string	"__UINT16_MAX__ 0xffff"
.LASF204:
	.string	"__GCC_ATOMIC_WCHAR_T_LOCK_FREE 2"
.LASF67:
	.string	"__UINT_FAST64_TYPE__ long unsigned int"
.LASF123:
	.string	"__UINTPTR_MAX__ 0xffffffffffffffffUL"
.LASF33:
	.string	"__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__"
.LASF108:
	.string	"__UINT_LEAST16_MAX__ 0xffff"
.LASF212:
	.string	"__PRAGMA_REDEFINE_EXTNAME 1"
.LASF53:
	.string	"__INT_LEAST16_TYPE__ short int"
.LASF18:
	.string	"__LP64__ 1"
.LASF163:
	.string	"__DECIMAL_DIG__ 21"
.LASF71:
	.string	"__has_include_next(STR) __has_include_next__(STR)"
.LASF122:
	.string	"__INTPTR_MAX__ 0x7fffffffffffffffL"
.LASF234:
	.string	"__gnu_linux__ 1"
.LASF150:
	.string	"__DBL_MAX__ ((double)1.79769313486231570815e+308L)"
.LASF45:
	.string	"__INT16_TYPE__ short int"
.LASF170:
	.string	"__LDBL_HAS_QUIET_NAN__ 1"
.LASF24:
	.string	"__SIZEOF_DOUBLE__ 8"
.LASF94:
	.string	"__UINT8_MAX__ 0xff"
.LASF17:
	.string	"_LP64 1"
.LASF51:
	.string	"__UINT64_TYPE__ long unsigned int"
.LASF54:
	.string	"__INT_LEAST32_TYPE__ int"
.LASF93:
	.string	"__INT64_MAX__ 0x7fffffffffffffffL"
.LASF224:
	.string	"__ATOMIC_HLE_RELEASE 131072"
.LASF230:
	.string	"__SSE2__ 1"
.LASF85:
	.string	"__INTMAX_C(c) c ## L"
.LASF203:
	.string	"__GCC_ATOMIC_CHAR32_T_LOCK_FREE 2"
.LASF213:
	.string	"__SIZEOF_INT128__ 16"
.LASF36:
	.string	"__PTRDIFF_TYPE__ long int"
.LASF174:
	.string	"__DEC32_MIN__ 1E-95DF"
.LASF99:
	.string	"__INT8_C(c) c"
.LASF21:
	.string	"__SIZEOF_LONG_LONG__ 8"
.LASF130:
	.string	"__FLT_DIG__ 6"
.LASF214:
	.string	"__SIZEOF_WCHAR_T__ 4"
.LASF105:
	.string	"__INT64_C(c) c ## L"
.LASF119:
	.string	"__UINT_FAST16_MAX__ 0xffffffffffffffffUL"
.LASF134:
	.string	"__FLT_MAX_10_EXP__ 38"
.LASF137:
	.string	"__FLT_MIN__ 1.17549435082228750797e-38F"
.LASF59:
	.string	"__UINT_LEAST64_TYPE__ long unsigned int"
.LASF25:
	.string	"__SIZEOF_LONG_DOUBLE__ 16"
.LASF73:
	.string	"__SCHAR_MAX__ 0x7f"
.LASF6:
	.string	"__GNUC_MINOR__ 1"
.LASF55:
	.string	"__INT_LEAST64_TYPE__ long int"
.LASF127:
	.string	"__DEC_EVAL_METHOD__ 2"
.LASF175:
	.string	"__DEC32_MAX__ 9.999999E96DF"
.LASF77:
	.string	"__LONG_LONG_MAX__ 0x7fffffffffffffffLL"
.LASF200:
	.string	"__GCC_ATOMIC_BOOL_LOCK_FREE 2"
.LASF35:
	.string	"__SIZE_TYPE__ long unsigned int"
.LASF159:
	.string	"__LDBL_MIN_EXP__ (-16381)"
.LASF125:
	.string	"__GCC_IEC_559_COMPLEX 2"
.LASF142:
	.string	"__FLT_HAS_QUIET_NAN__ 1"
.LASF152:
	.string	"__DBL_EPSILON__ ((double)2.22044604925031308085e-16L)"
.LASF32:
	.string	"__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__"
.LASF100:
	.string	"__INT_LEAST16_MAX__ 0x7fff"
.LASF83:
	.string	"__SIZE_MAX__ 0xffffffffffffffffUL"
.LASF112:
	.string	"__UINT_LEAST64_MAX__ 0xffffffffffffffffUL"
.LASF117:
	.string	"__INT_FAST64_MAX__ 0x7fffffffffffffffL"
.LASF30:
	.string	"__ORDER_BIG_ENDIAN__ 4321"
.LASF68:
	.string	"__INTPTR_TYPE__ long int"
.LASF160:
	.string	"__LDBL_MIN_10_EXP__ (-4931)"
.LASF210:
	.string	"__GCC_ATOMIC_POINTER_LOCK_FREE 2"
.LASF22:
	.string	"__SIZEOF_SHORT__ 2"
.LASF243:
	.string	"_STDC_PREDEF_H 1"
.LASF101:
	.string	"__INT16_C(c) c"
.LASF240:
	.string	"unix 1"
.LASF63:
	.string	"__INT_FAST64_TYPE__ long int"
.LASF182:
	.string	"__DEC64_MAX__ 9.999999999999999E384DD"
.LASF82:
	.string	"__PTRDIFF_MAX__ 0x7fffffffffffffffL"
.LASF138:
	.string	"__FLT_EPSILON__ 1.19209289550781250000e-7F"
.LASF75:
	.string	"__INT_MAX__ 0x7fffffff"
.LASF208:
	.string	"__GCC_ATOMIC_LLONG_LOCK_FREE 2"
.LASF104:
	.string	"__INT_LEAST64_MAX__ 0x7fffffffffffffffL"
.LASF186:
	.string	"__DEC128_MIN_EXP__ (-6142)"
.LASF13:
	.string	"__ATOMIC_RELEASE 3"
.LASF149:
	.string	"__DBL_DECIMAL_DIG__ 17"
.LASF144:
	.string	"__DBL_DIG__ 15"
.LASF135:
	.string	"__FLT_DECIMAL_DIG__ 9"
.LASF11:
	.string	"__ATOMIC_SEQ_CST 5"
.LASF188:
	.string	"__DEC128_MIN__ 1E-6143DL"
.LASF28:
	.string	"__BIGGEST_ALIGNMENT__ 16"
.LASF19:
	.string	"__SIZEOF_INT__ 4"
.LASF62:
	.string	"__INT_FAST32_TYPE__ long int"
.LASF206:
	.string	"__GCC_ATOMIC_INT_LOCK_FREE 2"
.LASF187:
	.string	"__DEC128_MAX_EXP__ 6145"
.LASF91:
	.string	"__INT16_MAX__ 0x7fff"
.LASF198:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1"
.LASF180:
	.string	"__DEC64_MAX_EXP__ 385"
.LASF14:
	.string	"__ATOMIC_ACQ_REL 4"
.LASF129:
	.string	"__FLT_MANT_DIG__ 24"
.LASF5:
	.string	"__GNUC__ 5"
.LASF40:
	.string	"__UINTMAX_TYPE__ long unsigned int"
.LASF245:
	.string	"__STDC_IEC_559_COMPLEX__ 1"
.LASF205:
	.string	"__GCC_ATOMIC_SHORT_LOCK_FREE 2"
.LASF189:
	.string	"__DEC128_MAX__ 9.999999999999999999999999999999999E6144DL"
.LASF9:
	.string	"__GNUC_RH_RELEASE__ 4"
.LASF57:
	.string	"__UINT_LEAST16_TYPE__ short unsigned int"
.LASF69:
	.string	"__UINTPTR_TYPE__ long unsigned int"
.LASF147:
	.string	"__DBL_MAX_EXP__ 1024"
.LASF0:
	.string	"__STDC__ 1"
.LASF241:
	.string	"__ELF__ 1"
.LASF126:
	.string	"__FLT_EVAL_METHOD__ 0"
.LASF84:
	.string	"__INTMAX_MAX__ 0x7fffffffffffffffL"
.LASF12:
	.string	"__ATOMIC_ACQUIRE 2"
.LASF66:
	.string	"__UINT_FAST32_TYPE__ long unsigned int"
.LASF102:
	.string	"__INT_LEAST32_MAX__ 0x7fffffff"
.LASF201:
	.string	"__GCC_ATOMIC_CHAR_LOCK_FREE 2"
.LASF190:
	.string	"__DEC128_EPSILON__ 1E-33DL"
.LASF64:
	.string	"__UINT_FAST8_TYPE__ unsigned char"
.LASF61:
	.string	"__INT_FAST16_TYPE__ long int"
.LASF172:
	.string	"__DEC32_MIN_EXP__ (-94)"
.LASF92:
	.string	"__INT32_MAX__ 0x7fffffff"
.LASF158:
	.string	"__LDBL_DIG__ 18"
.LASF244:
	.string	"__STDC_IEC_559__ 1"
.LASF191:
	.string	"__DEC128_SUBNORMAL_MIN__ 0.000000000000000000000000000000001E-6143DL"
.LASF164:
	.string	"__LDBL_MAX__ 1.18973149535723176502e+4932L"
.LASF223:
	.string	"__ATOMIC_HLE_ACQUIRE 65536"
.LASF232:
	.string	"__SSE_MATH__ 1"
.LASF115:
	.string	"__INT_FAST16_MAX__ 0x7fffffffffffffffL"
.LASF151:
	.string	"__DBL_MIN__ ((double)2.22507385850720138309e-308L)"
.LASF185:
	.string	"__DEC128_MANT_DIG__ 34"
.LASF103:
	.string	"__INT32_C(c) c"
.LASF179:
	.string	"__DEC64_MIN_EXP__ (-382)"
.LASF79:
	.string	"__WCHAR_MIN__ (-__WCHAR_MAX__ - 1)"
.LASF197:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1"
.LASF161:
	.string	"__LDBL_MAX_EXP__ 16384"
.LASF226:
	.string	"__k8__ 1"
.LASF42:
	.string	"__CHAR32_TYPE__ unsigned int"
.LASF86:
	.string	"__UINTMAX_MAX__ 0xffffffffffffffffUL"
.LASF76:
	.string	"__LONG_MAX__ 0x7fffffffffffffffL"
.LASF52:
	.string	"__INT_LEAST8_TYPE__ signed char"
.LASF109:
	.string	"__UINT16_C(c) c"
.LASF74:
	.string	"__SHRT_MAX__ 0x7fff"
.LASF23:
	.string	"__SIZEOF_FLOAT__ 4"
.LASF199:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1"
.LASF78:
	.string	"__WCHAR_MAX__ 0x7fffffff"
.LASF166:
	.string	"__LDBL_EPSILON__ 1.08420217248550443401e-19L"
.LASF31:
	.string	"__ORDER_PDP_ENDIAN__ 3412"
.LASF29:
	.string	"__ORDER_LITTLE_ENDIAN__ 1234"
.LASF251:
	.string	"GNU C11 5.1.1 20150618 (Red Hat 5.1.1-4) -mtune=generic -march=x86-64 -g3 -O0"
.LASF238:
	.string	"__unix 1"
.LASF10:
	.string	"__ATOMIC_RELAXED 0"
.LASF80:
	.string	"__WINT_MAX__ 0xffffffffU"
.LASF220:
	.string	"__x86_64__ 1"
.LASF171:
	.string	"__DEC32_MANT_DIG__ 7"
.LASF183:
	.string	"__DEC64_EPSILON__ 1E-15DD"
.LASF124:
	.string	"__GCC_IEC_559 2"
.LASF120:
	.string	"__UINT_FAST32_MAX__ 0xffffffffffffffffUL"
.LASF228:
	.string	"__MMX__ 1"
.LASF131:
	.string	"__FLT_MIN_EXP__ (-125)"
.LASF72:
	.string	"__GXX_ABI_VERSION 1008"
.LASF88:
	.string	"__SIG_ATOMIC_MAX__ 0x7fffffff"
.LASF247:
	.string	"__STDC_NO_THREADS__ 1"
.LASF65:
	.string	"__UINT_FAST16_TYPE__ long unsigned int"
.LASF167:
	.string	"__LDBL_DENORM_MIN__ 3.64519953188247460253e-4951L"
.LASF141:
	.string	"__FLT_HAS_INFINITY__ 1"
.LASF116:
	.string	"__INT_FAST32_MAX__ 0x7fffffffffffffffL"
.LASF44:
	.string	"__INT8_TYPE__ signed char"
.LASF87:
	.string	"__UINTMAX_C(c) c ## UL"
.LASF2:
	.string	"__STDC_UTF_16__ 1"
.LASF39:
	.string	"__INTMAX_TYPE__ long int"
.LASF195:
	.string	"__NO_INLINE__ 1"
.LASF168:
	.string	"__LDBL_HAS_DENORM__ 1"
.LASF70:
	.string	"__has_include(STR) __has_include__(STR)"
.LASF202:
	.string	"__GCC_ATOMIC_CHAR16_T_LOCK_FREE 2"
.LASF221:
	.string	"__SIZEOF_FLOAT80__ 16"
.LASF192:
	.string	"__REGISTER_PREFIX__ "
.LASF15:
	.string	"__ATOMIC_CONSUME 1"
.LASF97:
	.string	"__UINT64_MAX__ 0xffffffffffffffffUL"
.LASF3:
	.string	"__STDC_UTF_32__ 1"
.LASF49:
	.string	"__UINT16_TYPE__ short unsigned int"
.LASF229:
	.string	"__SSE__ 1"
.LASF111:
	.string	"__UINT32_C(c) c ## U"
.LASF225:
	.string	"__k8 1"
.LASF177:
	.string	"__DEC32_SUBNORMAL_MIN__ 0.000001E-95DF"
.LASF249:
	.string	"frame2"
.LASF248:
	.string	"frame3"
.LASF216:
	.string	"__SIZEOF_PTRDIFF_T__ 8"
.LASF27:
	.string	"__CHAR_BIT__ 8"
.LASF60:
	.string	"__INT_FAST8_TYPE__ signed char"
.LASF89:
	.string	"__SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)"
.LASF176:
	.string	"__DEC32_EPSILON__ 1E-6DF"
.LASF58:
	.string	"__UINT_LEAST32_TYPE__ unsigned int"
.LASF154:
	.string	"__DBL_HAS_DENORM__ 1"
.LASF227:
	.string	"__code_model_small__ 1"
.LASF157:
	.string	"__LDBL_MANT_DIG__ 64"
.LASF207:
	.string	"__GCC_ATOMIC_LONG_LOCK_FREE 2"
.LASF242:
	.string	"__DECIMAL_BID_FORMAT__ 1"
.LASF90:
	.string	"__INT8_MAX__ 0x7f"
.LASF118:
	.string	"__UINT_FAST8_MAX__ 0xff"
.LASF132:
	.string	"__FLT_MIN_10_EXP__ (-37)"
.LASF209:
	.string	"__GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1"
.LASF37:
	.string	"__WCHAR_TYPE__ int"
.LASF16:
	.string	"__FINITE_MATH_ONLY__ 0"
.LASF193:
	.string	"__USER_LABEL_PREFIX__ "
.LASF41:
	.string	"__CHAR16_TYPE__ short unsigned int"
.LASF107:
	.string	"__UINT8_C(c) c"
.LASF219:
	.string	"__x86_64 1"
.LASF56:
	.string	"__UINT_LEAST8_TYPE__ unsigned char"
.LASF47:
	.string	"__INT64_TYPE__ long int"
.LASF196:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1"
.LASF250:
	.string	"main"
.LASF96:
	.string	"__UINT32_MAX__ 0xffffffffU"
.LASF211:
	.string	"__GCC_HAVE_DWARF2_CFI_ASM 1"
.LASF98:
	.string	"__INT_LEAST8_MAX__ 0x7f"
.LASF181:
	.string	"__DEC64_MIN__ 1E-383DD"
.LASF146:
	.string	"__DBL_MIN_10_EXP__ (-307)"
.LASF139:
	.string	"__FLT_DENORM_MIN__ 1.40129846432481707092e-45F"
.LASF194:
	.string	"__GNUC_STDC_INLINE__ 1"
	.ident	"GCC: (GNU) 5.1.1 20150618 (Red Hat 5.1.1-4)"
	.section	.note.GNU-stack,"",@progbits
