
program main

  interface
     subroutine assumed_size_2d(name, arr)
       character*(*) :: name
       integer :: arr(10,*)
     end subroutine assumed_size_2d
  end interface

  ! Static storage with 2 dimensions.
  integer, dimension(-2:2,-3:3), target :: storage1_2d

  storage1_2d = -1
  storage1_2d(-2,-3) = -2
  storage1_2d(2,3) = -3

  call assumed_size_2d('storage1_2d', storage1_2d)
end program main

subroutine assumed_size_2d(name, arr)
  implicit none

  character*(*) :: name
  integer :: arr(10,*)

  ! Breakpoint

end subroutine assumed_size_2d
