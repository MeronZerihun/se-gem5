BENCHMARK_DIRS="boyer-moore-search bubble-sort edit-distance eulers-number-approx fft-int kepler-calc mersenne mnist-cnn nonlinear-nn nr-solver parrando rad-to-degree scalar-gwas-chi2 tea-cipher vector-gwas-chi2"
#BENCHMARK_DIRS="boyer-moore-search bubble-sort"


for dir in  $BENCHMARK_DIRS; do
    cp templates/* templates-replace/
    grep -lR "FIXME" templates-replace/ | xargs sed -i "s/FIXME/$dir/g"
    cp templates-replace/FIXME-enc.py $dir-enc.py
    cp templates-replace/FIXME-native.py $dir-native.py
    cp templates-replace/FIXME-do.py $dir-do.py
done
