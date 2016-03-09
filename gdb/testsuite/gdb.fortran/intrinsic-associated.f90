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

program intrinsic_associated
  integer, allocatable, target :: vla (:), vla2 (:)
  integer, pointer :: pvla (:),  pvla2 (:)
  integer, pointer :: value
  logical :: l
  integer, dimension (1:10), target :: array
  allocate(vla(10))
  allocate(vla2(10))

  vla = (/ (I, I = 1,10) /)
  value => vla(10)

  l = associated (value)

  value => null()  !  value associated

  l = associated (value)

  vla(10) = vla(10) + 1 !  value desassociated

  pvla => vla(:)

  l  = associated (pvla, vla)

  pvla => vla(10:1:-1)  !  pvla-vla associated

  l = associated (pvla, vla)

  vla(10) = vla(10) + 1 !  pvla-vla desassociated

  pvla2 => pvla

  l = associated (pvla2, pvla)

  vla(10) = vla(10) + 1 !  associated pvla2-pvla

  pvla => null()

  l = associated (pvla, vla)

  vla(10) = vla(10) + 1 !  desassociated pvla2-pvla

  pvla => array

  l = associated (pvla, array)

  vla(10) = vla(10) + 1 !  associated pvla-array

end program intrinsic_associated
