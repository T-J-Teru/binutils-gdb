! Copyright 2021 Free Software Foundation, Inc.
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; either version 3 of the License, or
! (at your option) any later version.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License
! along with this program.  If not, see <http://www.gnu.org/licenses/>.

!
! Start of test program.
!
program test

  ! Declare variables used in this test.
  integer, dimension (1:5) :: array_1d
  integer, dimension (1:5, 1:5) :: array_2d
  integer, dimension (1:5, 1:5, 1:5) :: array_3d

  array_1d = 1
  array_2d = 2
  array_3d = 3

  print *, ""		! Break Here
  print *, array_1d
  print *, array_2d
  print *, array_3d

end program test
