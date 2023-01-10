cd release
rm tSari.tar.xz readme -rf
tar cvf tSari.tar *
xz -z  tSari.tar tSari.tar.xz
date1=`date +'%Y%m%d%H%M'_`
ver=`strings carplate_main | grep "COMMON_V"`_
pass=$(md5sum tSari.tar.xz)
pre='CARPLATE'
str=${pre}${date1}${ver}${pass}
echo $str>readme
