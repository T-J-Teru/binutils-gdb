program simple
implicit none
integer :: i
logical, dimension (4,2) :: truth_table
truth_table(1, 1) = .FALSE.
truth_table(1, 2) = .FALSE.
truth_table(2, 1) = .FALSE.
truth_table(2, 2) = .TRUE.
truth_table(3, 1) = .TRUE.
truth_table(3, 2) = .FALSE.
truth_table(4, 1) = .TRUE.
truth_table(4, 2) = .TRUE.

! breakpoint1
print *, truth_table(2, 1) .OR. truth_table(2, 2)

end program simple
