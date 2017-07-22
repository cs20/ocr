BEGIN {
}

{

# Create the arrays after filtering the data and merging them
# 1. DB size > 1M
# 2. No. of invocations > 1

 if (($1 ~ /^0x/) && ($7 >= 1048576) && ($2 > 1)) {

    if($1 in count) {
        # Merge
        accesses[$1] = (accesses[$1]*count[$1] + $7*$2*$2)/(count[$1]+$2)
        dbsize[$1] = (dbsize[$1]*count[$1] + $7*$2)/(count[$1]+$2)
        hbm[$1] = (hbm[$1]*count[$1] + ($5*$2/$4))/(count[$1]+$2)
        offcore[$1] = (offcore[$1]*count[$1] + ($3*$2/$5))/(count[$1]+$2)
        count[$1] += $2
    } else {
        count[$1] = $2
        accesses[$1] = $7*$2
        dbsize[$1] = $7
        hbm[$1] = $5/$4
        offcore[$1] = $3/$5
    }
 }


# Sort by:
# 1. Sort by |DB size| * |No. of invocations|  first
# 2. Sort by |DB size| next
# 3. Sort by Offcore_traffic/HBM_traffic
# 4. Reverse sort by HWCycles/Offcore_traffic


}

END{
 for (var in count) {
    print var, count[var], accesses[var], dbsize[var], hbm[var], offcore[var]
 }

}
