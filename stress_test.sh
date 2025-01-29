#!/bin/bash
for i in {0..1}; do
    (
        for j in {1..100}; do
            echo "Dev$i-Write$j" > /dev/static_chardev$i
            cat /dev/static_chardev$i | grep "Dev$i-Write$j" || echo "Error in dev$i"
        done
    ) &
done
wait
