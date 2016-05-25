program internalvar
  integer, dimension(:), allocatable :: arrayvar

  allocate (arrayvar (10))
  arrayvar = 3

  ! Breakpoint 1
end program internalvar
