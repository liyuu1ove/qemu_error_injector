SEED=122
mkdir -p "aes_result"
rm -f aes_result/test_AES_result_${SEED}.txt
touch aes_result/test_AES_result_${SEED}.txt
for prob in {0..100}
do
    bash run_all_tests.sh  -S ${SEED} -p ${prob} -m 0 -t test_AES -v &>> aes_result/test_AES_result_${SEED}.txt
done