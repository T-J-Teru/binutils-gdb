recursive function factorial(n) result(res)
  implicit none
  integer :: n, res

  if (n .eq. 0) then
     res = 1
  else
     res = n * factorial(n - 1)
  end if
end function factorial

program demofactorial
  integer, external :: factorial
  ! breakpoint1
  print *, "The value of 5 factorial is ", factorial(5)
  
end program demofactorial
