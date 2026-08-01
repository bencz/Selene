	.file	"elena"
	.text
	.globl	elena.sym.helloworld.program
	.p2align	4
	.type	elena.sym.helloworld.program,@function
elena.sym.helloworld.program:
	leaq	elena.k.02.000001(%rip), %rax
	retq
.Lfunc_end0:
	.size	elena.sym.helloworld.program, .Lfunc_end0-elena.sym.helloworld.program

	.globl	elena_program
	.p2align	4
	.type	elena_program,@function
elena_program:
	leaq	elena.k.02.000001(%rip), %rax
	retq
.Lfunc_end1:
	.size	elena_program, .Lfunc_end1-elena_program

	.type	elena.vmt.system.LiteralValue,@object
	.bss
	.weak	elena.vmt.system.LiteralValue
	.p2align	3, 0x0
elena.vmt.system.LiteralValue:
	.quad	0
	.size	elena.vmt.system.LiteralValue, 8

	.type	.Lelena.k.02.000001.image,@object
	.data
	.p2align	4, 0x0
.Lelena.k.02.000001.image:
	.quad	-24
	.quad	elena.vmt.system.LiteralValue
	.asciz	"Hello World from ELENA!"
	.size	.Lelena.k.02.000001.image, 40

elena.k.02.000001 = .Lelena.k.02.000001.image+16
	.size	elena.k.02.000001, 24
	.section	".note.GNU-stack","",@progbits
