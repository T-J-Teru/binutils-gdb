	call.s VALUE
	call.s c,VALUE
	call.s m,VALUE
	call.s nc,VALUE
	call.s nz,VALUE
	call.s p,VALUE
	call.s pe,VALUE
	call.s po,VALUE
	call.s z,VALUE
	jp.s VALUE
	jp.s c,VALUE
	jp.s m,VALUE
	jp.s nc,VALUE
	jp.s nz,VALUE
	jp.s p,VALUE
	jp.s pe,VALUE
	jp.s po,VALUE
	jp.s z,VALUE
	ld.s (VALUE),a
	ld.s (VALUE),bc
	ld.s (VALUE),de
	ld.s (VALUE),hl
	ld.s (VALUE),ix
	ld.s (VALUE),iy
	ld.s (VALUE),sp
	ld.s a,(VALUE)
	ld.s bc,(VALUE)
	ld.s bc,VALUE
	ld.s de,(VALUE)
	ld.s de,VALUE
	ld.s hl,(VALUE)
	ld.s hl,VALUE
	ld.s ix,(VALUE)
	ld.s ix,VALUE
	ld.s iy,(VALUE)
	ld.s iy,VALUE
	ld.s sp,(VALUE)
	ld.s sp,VALUE

	.page
	call.l VALUE
	call.l c,VALUE
	call.l m,VALUE
	call.l nc,VALUE
	call.l nz,VALUE
	call.l p,VALUE
	call.l pe,VALUE
	call.l po,VALUE
	call.l z,VALUE
	jp.l VALUE
	jp.l c,VALUE
	jp.l m,VALUE
	jp.l nc,VALUE
	jp.l nz,VALUE
	jp.l p,VALUE
	jp.l pe,VALUE
	jp.l po,VALUE
	jp.l z,VALUE
	ld.l (VALUE),a
	ld.l (VALUE),bc
	ld.l (VALUE),de
	ld.l (VALUE),hl
	ld.l (VALUE),ix
	ld.l (VALUE),iy
	ld.l (VALUE),sp
	ld.l a,(VALUE)
	ld.l bc,(VALUE)
	ld.l bc,VALUE
	ld.l de,(VALUE)
	ld.l de,VALUE
	ld.l hl,(VALUE)
	ld.l hl,VALUE
	ld.l ix,(VALUE)
	ld.l ix,VALUE
	ld.l iy,(VALUE)
	ld.l iy,VALUE
	ld.l sp,(VALUE)
	ld.l sp,VALUE

	.page
	call.is 0x3456
	call.is c,0x3456
	call.is m,0x3456
	call.is nc,0x3456
	call.is nz,0x3456
	call.is p,0x3456
	call.is pe,0x3456
	call.is po,0x3456
	call.is z,0x3456
	jp.is 0x3456
	jp.is c,0x3456
	jp.is m,0x3456
	jp.is nc,0x3456
	jp.is nz,0x3456
	jp.is p,0x3456
	jp.is pe,0x3456
	jp.is po,0x3456
	jp.is z,0x3456
	ld.is (0x3456),a
	ld.is (0x3456),bc
	ld.is (0x3456),de
	ld.is (0x3456),hl
	ld.is (0x3456),ix
	ld.is (0x3456),iy
	ld.is (0x3456),sp
	ld.is a,(0x3456)
	ld.is bc,(0x3456)
	ld.is bc,0x3456
	ld.is de,(0x3456)
	ld.is de,0x3456
	ld.is hl,(0x3456)
	ld.is hl,0x3456
	ld.is ix,(0x3456)
	ld.is ix,0x3456
	ld.is iy,(0x3456)
	ld.is iy,0x3456
	ld.is sp,(0x3456)
	ld.is sp,0x3456

	.page
	call.il 0x123456
	call.il c,0x123456
	call.il m,0x123456
	call.il nc,0x123456
	call.il nz,0x123456
	call.il p,0x123456
	call.il pe,0x123456
	call.il po,0x123456
	call.il z,0x123456
	jp.il 0x123456
	jp.il c,0x123456
	jp.il m,0x123456
	jp.il nc,0x123456
	jp.il nz,0x123456
	jp.il p,0x123456
	jp.il pe,0x123456
	jp.il po,0x123456
	jp.il z,0x123456
	ld.il (0x123456),a
	ld.il (0x123456),bc
	ld.il (0x123456),de
	ld.il (0x123456),hl
	ld.il (0x123456),ix
	ld.il (0x123456),iy
	ld.il (0x123456),sp
	ld.il a,(0x123456)
	ld.il bc,(0x123456)
	ld.il bc,0x123456
	ld.il de,(0x123456)
	ld.il de,0x123456
	ld.il hl,(0x123456)
	ld.il hl,0x123456
	ld.il ix,(0x123456)
	ld.il ix,0x123456
	ld.il iy,(0x123456)
	ld.il iy,0x123456
	ld.il sp,(0x123456)
	ld.il sp,0x123456

	.page
	call.sis 0x3456
	call.sis c,0x3456
	call.sis m,0x3456
	call.sis nc,0x3456
	call.sis nz,0x3456
	call.sis p,0x3456
	call.sis pe,0x3456
	call.sis po,0x3456
	call.sis z,0x3456
	jp.sis 0x3456
	jp.sis c,0x3456
	jp.sis m,0x3456
	jp.sis nc,0x3456
	jp.sis nz,0x3456
	jp.sis p,0x3456
	jp.sis pe,0x3456
	jp.sis po,0x3456
	jp.sis z,0x3456
	ld.sis (0x3456),a
	ld.sis (0x3456),bc
	ld.sis (0x3456),de
	ld.sis (0x3456),hl
	ld.sis (0x3456),ix
	ld.sis (0x3456),iy
	ld.sis (0x3456),sp
	ld.sis a,(0x3456)
	ld.sis bc,(0x3456)
	ld.sis bc,0x3456
	ld.sis de,(0x3456)
	ld.sis de,0x3456
	ld.sis hl,(0x3456)
	ld.sis hl,0x3456
	ld.sis ix,(0x3456)
	ld.sis ix,0x3456
	ld.sis iy,(0x3456)
	ld.sis iy,0x3456
	ld.sis sp,(0x3456)
	ld.sis sp,0x3456

	.page
	call.lis 0x3456
	call.lis c,0x3456
	call.lis m,0x3456
	call.lis nc,0x3456
	call.lis nz,0x3456
	call.lis p,0x3456
	call.lis pe,0x3456
	call.lis po,0x3456
	call.lis z,0x3456
	jp.lis 0x3456
	jp.lis c,0x3456
	jp.lis m,0x3456
	jp.lis nc,0x3456
	jp.lis nz,0x3456
	jp.lis p,0x3456
	jp.lis pe,0x3456
	jp.lis po,0x3456
	jp.lis z,0x3456
	ld.lis (0x3456),a
	ld.lis (0x3456),bc
	ld.lis (0x3456),de
	ld.lis (0x3456),hl
	ld.lis (0x3456),ix
	ld.lis (0x3456),iy
	ld.lis (0x3456),sp
	ld.lis a,(0x3456)
	ld.lis bc,(0x3456)
	ld.lis bc,0x3456
	ld.lis de,(0x3456)
	ld.lis de,0x3456
	ld.lis hl,(0x3456)
	ld.lis hl,0x3456
	ld.lis ix,(0x3456)
	ld.lis ix,0x3456
	ld.lis iy,(0x3456)
	ld.lis iy,0x3456
	ld.lis sp,(0x3456)
	ld.lis sp,0x3456

	.page
	call.sil 0x123456
	call.sil c,0x123456
	call.sil m,0x123456
	call.sil nc,0x123456
	call.sil nz,0x123456
	call.sil p,0x123456
	call.sil pe,0x123456
	call.sil po,0x123456
	call.sil z,0x123456
	jp.sil 0x123456
	jp.sil c,0x123456
	jp.sil m,0x123456
	jp.sil nc,0x123456
	jp.sil nz,0x123456
	jp.sil p,0x123456
	jp.sil pe,0x123456
	jp.sil po,0x123456
	jp.sil z,0x123456
	ld.sil (0x123456),a
	ld.sil (0x123456),bc
	ld.sil (0x123456),de
	ld.sil (0x123456),hl
	ld.sil (0x123456),ix
	ld.sil (0x123456),iy
	ld.sil (0x123456),sp
	ld.sil a,(0x123456)
	ld.sil bc,(0x123456)
	ld.sil bc,0x123456
	ld.sil de,(0x123456)
	ld.sil de,0x123456
	ld.sil hl,(0x123456)
	ld.sil hl,0x123456
	ld.sil ix,(0x123456)
	ld.sil ix,0x123456
	ld.sil iy,(0x123456)
	ld.sil iy,0x123456
	ld.sil sp,(0x123456)
	ld.sil sp,0x123456

	.page
	call.lil 0x123456
	call.lil c,0x123456
	call.lil m,0x123456
	call.lil nc,0x123456
	call.lil nz,0x123456
	call.lil p,0x123456
	call.lil pe,0x123456
	call.lil po,0x123456
	call.lil z,0x123456
	jp.lil 0x123456
	jp.lil c,0x123456
	jp.lil m,0x123456
	jp.lil nc,0x123456
	jp.lil nz,0x123456
	jp.lil p,0x123456
	jp.lil pe,0x123456
	jp.lil po,0x123456
	jp.lil z,0x123456
	ld.lil (0x123456),a
	ld.lil (0x123456),bc
	ld.lil (0x123456),de
	ld.lil (0x123456),hl
	ld.lil (0x123456),ix
	ld.lil (0x123456),iy
	ld.lil (0x123456),sp
	ld.lil a,(0x123456)
	ld.lil bc,(0x123456)
	ld.lil bc,0x123456
	ld.lil de,(0x123456)
	ld.lil de,0x123456
	ld.lil hl,(0x123456)
	ld.lil hl,0x123456
	ld.lil ix,(0x123456)
	ld.lil ix,0x123456
	ld.lil iy,(0x123456)
	ld.lil iy,0x123456
	ld.lil sp,(0x123456)
	ld.lil sp,0x123456
