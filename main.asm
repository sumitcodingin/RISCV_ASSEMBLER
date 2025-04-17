  .text 
   
    addi x5, x0, 4
    addi x6, x0, 1
fact_loop:
    beq x5, x0, fact_end
    mul x6, x6, x5
    addi x5, x5, -1
    jal x0, fact_loop
fact_end: