
.data
arr: .word -20, -15, -10, -5, 0, 5, 10, 15, 20, 25
key: .word -10   # Change this value to test other keys

.text

main:
    # Initialize base address of array
    lui x20, 0x10000           # x20 = base of array
    addi x21, x0, 0            # x21 = low = 0
    addi x22, x0, 9            # x22 = high = 9 (n-1)

    # Load key to search
    lui x23, 0x10000
    addi x23, x23, 40          # offset to 'key'
    lw x24, 0(x23)             # x24 = key

binary_search:
    blt x22, x21, not_found    # if high < low → not found

    add x25, x21, x22          # x25 = low + high
    srli x25, x25, 1           # x25 = mid

    slli x26, x25, 2           # x26 = mid * 4
    add x27, x20, x26          # x27 = address of arr[mid]
    lw x28, 0(x27)             # x28 = arr[mid]

    beq x24, x28, found        # if key == arr[mid]
    blt x24, x28, go_left      # if key < arr[mid]

    # Go right
    addi x21, x25, 1
    jal x0, binary_search

go_left:
    addi x22, x25, -1
    jal x0, binary_search

found:
    add x10, x0, x25           # store result in x10
    jal x0, end

not_found:
    addi x10, x0, -1           # key not found

end:
    addi x0, x0, 0             # nop to halt for Venus