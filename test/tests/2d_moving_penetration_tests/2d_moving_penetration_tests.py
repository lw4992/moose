from options import *

#This is a set of 5 basic tests of the penetration locator with a moving
#mesh.  The baseline cases are with linear quad4 elements.  The 'q' postfix
#on the case names indicates variants with quadratic quad8 elements.  The
#'tt' postfix indicates variants with a nonzero tangential tolerance.

pl_test1 = { INPUT : 'pl_test1.i',
             EXODIFF : ['pl_test1_out.e'] }

pl_test1tt = { INPUT : 'pl_test1tt.i',
               EXODIFF : ['pl_test1tt_out.e'] }

pl_test1q = { INPUT : 'pl_test1q.i',
              EXODIFF : ['pl_test1q_out.e'] }

pl_test1qtt = { INPUT : 'pl_test1qtt.i',
                EXODIFF : ['pl_test1qtt_out.e'] }

pl_test2 = { INPUT : 'pl_test2.i',
             EXODIFF : ['pl_test2_out.e'] }

pl_test2tt = { INPUT : 'pl_test2tt.i',
               EXODIFF : ['pl_test2tt_out.e'] }

pl_test2q = { INPUT : 'pl_test2q.i',
              EXODIFF : ['pl_test2q_out.e'] }

pl_test2qtt = { INPUT : 'pl_test2qtt.i',
                EXODIFF : ['pl_test2qtt_out.e'] }

pl_test3 = { INPUT : 'pl_test3.i',
             EXODIFF : ['pl_test3_out.e'] }

pl_test3tt = { INPUT : 'pl_test3tt.i',
               EXODIFF : ['pl_test3tt_out.e'] }

pl_test3q = { INPUT : 'pl_test3q.i',
              EXODIFF : ['pl_test3q_out.e'] }

pl_test3qtt = { INPUT : 'pl_test3qtt.i',
                EXODIFF : ['pl_test3qtt_out.e'] }

pl_test4 = { INPUT : 'pl_test4.i',
             EXODIFF : ['pl_test4_out.e'] }

pl_test4tt = { INPUT : 'pl_test4tt.i',
               EXODIFF : ['pl_test4tt_out.e'] }

pl_test4q = { INPUT : 'pl_test4q.i',
              EXODIFF : ['pl_test4q_out.e'] }

pl_test4qtt = { INPUT : 'pl_test4qtt.i',
                EXODIFF : ['pl_test4qtt_out.e'] }

pl_test5 = { INPUT : 'pl_test5.i',
             EXODIFF : ['pl_test5_out.e'] }

pl_test5tt = { INPUT : 'pl_test5tt.i',
               EXODIFF : ['pl_test5tt_out.e'] }

pl_test5q = { INPUT : 'pl_test5q.i',
              EXODIFF : ['pl_test5q_out.e'] }

pl_test5qtt = { INPUT : 'pl_test5qtt.i',
                EXODIFF : ['pl_test5qtt_out.e'] }