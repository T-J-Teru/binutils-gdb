program test

interface 
	subroutine assumed(name, arr)
		character*(*) :: name
		integer :: arr(:)
	end subroutine
	subroutine assumed_shape_2d(name, arr)
		character*(*) :: name
		integer :: arr(:,:)
	end subroutine
	subroutine assumed_size_2d(name, arr)
		character*(*) :: name
		integer :: arr(10,*)
	end subroutine
	subroutine assumed_size_other_rank(name, arr)
		character*(*) :: name
		integer :: arr(*)
	end subroutine
end interface 

!static storage
integer, dimension(10), target :: storage1

!pointer to storage1
integer, dimension(:), pointer :: pointer1

!dynamically allocated
integer, dimension(:), allocatable, target :: storage2

!pointer to storage2
integer, dimension(:), pointer :: pointer2

!static storage
integer, dimension(10) :: storage3

!static storage - 2 dimensions
integer, dimension(-2:2,-3:3), target :: storage1_2d

!pointer to storage1_2d
integer, dimension(:,:), pointer :: pointer1_2d

!dynamically allocated - 2 dimensions
integer, dimension(:,:), allocatable, target :: storage2_2d

!pointer to storage2_2d
integer, dimension(:,:), pointer :: pointer2_2d

!static storage - 2 dimensions
integer, dimension(-2:2,-3:3) :: storage3_2d

allocate(storage2(10))

storage1=-1
storage1(1)=-2
storage1(10)=-3

storage2=-4
storage2(1)=-5
storage2(10)=-6

pointer1=>storage1(3:8)

pointer2=>storage2(3:8)

pointer1=-7
pointer1(1)=-8
pointer1(6)=-9

pointer2=-10
pointer2(1)=-11
pointer2(6)=-12

storage3=-13
storage3(1)=-14
storage3(10)=-15

print *,"storage1 = ",storage1
print *,"storage2 = ",storage2
print *,"storage3 = ",storage3
print *,"pointer1 = ",pointer1
print *,"pointer2 = ",pointer2

call explicit1('storage1', storage1)
call explicit1('storage2', storage2)
call explicit1('storage3', storage3)
call explicit2('pointer1', pointer1)
call explicit2('pointer2', pointer2)
call assumed('storage1', storage1)
call assumed('storage2', storage2)
call assumed('storage3', storage3)
call assumed('pointer1', pointer1)
call assumed('pointer2', pointer2)
call automatic('storage1', 10, storage1)
call automatic('storage2', 10, storage2)
call automatic('storage3', 10, storage3)
call automatic('pointer1', 6, pointer1)
call automatic('pointer2', 6, pointer2)

allocate(storage2_2d(-2:2,-3:3))

storage1_2d=-1
storage1_2d(-2,-3)=-2
storage1_2d(2,3)=-3

storage2_2d=-4
storage2_2d(-2,-3)=-5
storage2_2d(2,3)=-6

pointer1_2d=>storage1_2d(-1:1,-2:2)

pointer2_2d=>storage2_2d(-1:1,-2:2)

pointer1_2d=-7
pointer1_2d(1,1)=-8
pointer1_2d(3,5)=-9

pointer2_2d=-10
pointer2_2d(1,1)=-11
pointer2_2d(3,5)=-12

storage3_2d=-13
storage3_2d(-2,-3)=-14
storage3_2d(2,3)=-15

print *,"storage1_2d = ",storage1_2d
print *,"storage2_2d = ",storage2_2d
print *,"storage3_2d = ",storage3_2d
print *,"pointer1_2d = ",pointer1_2d
print *,"pointer2_2d = ",pointer2_2d

call explicit1_2d('storage1_2d', storage1_2d)
call explicit1_2d('storage2_2d', storage2_2d)
call explicit1_2d('storage3_2d', storage3_2d)
call explicit2_2d('pointer1_2d', pointer1_2d)
call explicit2_2d('pointer2_2d', pointer2_2d)
call assumed_shape_2d('storage1_2d', storage1_2d)
call assumed_shape_2d('storage2_2d', storage2_2d)
call assumed_shape_2d('storage3_2d', storage3_2d)
call assumed_shape_2d('pointer1_2d', pointer1_2d)
call assumed_shape_2d('pointer2_2d', pointer2_2d)
call assumed_size_2d('storage1_2d', storage1_2d)
call assumed_size_2d('storage2_2d', storage2_2d)
call assumed_size_2d('storage3_2d', storage3_2d)
call assumed_size_2d('pointer1_2d', pointer1_2d)
call assumed_size_2d('pointer2_2d', pointer2_2d)
call assumed_size_other_rank('storage1_2d', storage1_2d)
call assumed_size_other_rank('storage2_2d', storage2_2d)
call assumed_size_other_rank('storage3_2d', storage3_2d)
call assumed_size_other_rank('pointer1_2d', pointer1_2d)
call assumed_size_other_rank('pointer2_2d', pointer2_2d)
call automatic_2d('storage1_2d', 5, 7, storage1_2d)
call automatic_2d('storage2_2d', 5, 7, storage2_2d)
call automatic_2d('storage3_2d', 5, 7, storage3_2d)
call automatic_2d('pointer1_2d', 3, 5, pointer1_2d)
call automatic_2d('pointer2_2d', 3, 5, pointer2_2d)

write(*,*) 'end of program'

end

subroutine explicit1(name, arr)

character*(*) :: name
integer :: arr(10)

print *,"explicit1: ",name," = ",arr ! BP_01

end

subroutine explicit2(name, arr)

character*(*) :: name
integer :: arr(6)

print *,"explicit2: ",name," = ",arr ! BP_02

end

subroutine assumed(name, arr)
implicit none

character*(*) :: name
integer :: arr(:)

print *,"assumed: ",name," = ",arr ! BP_03

end

subroutine automatic(name, x, arr)
implicit none

character*(*) :: name
integer :: x
integer :: arr(x)

print *,"automatic: ",name," = ",arr ! BP_04

end

subroutine explicit1_2d(name, arr)

character*(*) :: name
integer :: arr(5, 7)

print *,"explicit1_2d: ",name," = ",arr ! BP_05

end

subroutine explicit2_2d(name, arr)

character*(*) :: name
integer :: arr(3, 5)

print *,"explicit2_2d: ",name," = ",arr ! BP_06

end

subroutine assumed_shape_2d(name, arr)
implicit none

character*(*) :: name
integer :: arr(:,:)

print *,"assumed_shape_2d: ",name," = ",arr ! BP_07
print *, arr(1,1)
end

subroutine assumed_size_2d(name, arr)
implicit none

character*(*) :: name
integer :: arr(10,*)

print *,"assumed_size_2d: ",name," = ",arr(:,1) ! BP_08

end

subroutine assumed_size_other_rank(name, arr)
implicit none

character*(*) :: name
integer :: arr(*)

print *,"assumed_size_other_rank: ",name," = ",arr(5) ! BP_09

end

subroutine automatic_2d(name, x, y, arr)
implicit none

character*(*) :: name
integer :: x
integer :: y
integer :: arr(x,y)

print *,"automatic_2d: ",name," = ",arr ! BP_10

end
