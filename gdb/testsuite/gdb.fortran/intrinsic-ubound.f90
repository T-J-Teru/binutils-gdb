! Copyright 2015 Free Software Foundation, Inc.
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; either version 2 of the License, or
! (at your option) any later version.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License
! along with this program; if not, write to the Free Software
! Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

program intrinsic_ubound
  integer*1, allocatable :: ivla1d (:), ivla1d1bytes (:), ivla1d2bytes (:), ivla1d4bytes (:)
  integer*1, allocatable :: ivla2d (:, :)
  integer*1, allocatable :: ivla3d (:, :, :)
  integer*1, allocatable :: ivla7d (:, :, :, :, :, :, :)
  integer :: bound1(1), bound11
  integer :: bound2(2), bound21, bound22, bound2k8
  integer :: bound3(3), bound31, bound32, bound33
  integer :: bound1_1byte_overflow, bound1_2byte_overflow, bound1_4byte_overflow
  integer*1 :: ibound2(2), ibound21, ibound22, ibound2k8
  integer   :: sizefor11, sizefor32
  integer*8 :: byte1_overflow, byte2_overflow, byte4_overflow

  allocate (ivla1d (6:7))
  allocate (ivla2d (6:7, 298:300))
  allocate (ivla3d (1:7, 5:8, 4:9))
  allocate (ivla7d (1, 1, 1, 1, 1, 1, 1))

  byte1_overflow = 127 + 1
  byte2_overflow = 32767 + 1

  ! It seems to have an issue with the compiler gfortran while taking this line
  ! byte4_overflow = 2147483647 + 1
  byte4_overflow = 2147483647

  allocate (ivla1d1bytes (byte1_overflow : byte1_overflow + 2))
  allocate (ivla1d2bytes (byte2_overflow : byte2_overflow + 2))
  allocate (ivla1d4bytes (byte4_overflow + 1: byte4_overflow + 2))

  ivla1d(:) = 1
  ivla2d(:, :) = 2
  ivla3d(:, :, :) = 3
  ivla2d(4, 2) = 42
  bound1 = ubound (ivla1d)
  bound11 = ubound (ivla1d, 1)

  bound2 = ubound (ivla2d)
  bound21 = ubound (ivla2d, 1)
  bound22 = ubound (ivla2d, 2)
  bound22 = ubound (ivla2d, 2)
  ibound2k8 = ubound (ivla2d, 2, 1)

  bound3 = ubound  (ivla3d)
  bound31 = ubound (ivla3d, 1)
  bound32 = ubound (ivla3d, 2)
  bound33 = ubound (ivla3d, 3)

  sizefor11 = size (ivla2d, 2)
  bound1_1byte_overflow = ubound (ivla1d1bytes, 1, 1)
  bound1_2byte_overflow = ubound (ivla1d2bytes, 1, 2)
  bound1_4byte_overflow = ubound (ivla1d4bytes, 1, 4)

  write (*,*) bound1  ! all-assigned
  write (*,*) bound11
  write (*,*) bound2
  write (*,*) bound21
  write (*,*) bound22
  write (*,*) ibound2k8

end program intrinsic_ubound
