ls
/bin/ls > y
/bin/cat < y | /usr/bin/sort | /usr/bin/uniq | /usr/bin/wc > y1
cat y1
rm y1
ls |  sort | uniq | wc
rm y
