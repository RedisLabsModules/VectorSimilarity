
for bm_class in basics batch_iterator; do \
    for type in single multi; do \
        for data_type in fp32; do \
            echo ${bm_class}_${type}_${data_type}; \
        done \
    done \
done

echo updated_index_single_fp32
echo spaces_fp32
echo spaces_fp64
