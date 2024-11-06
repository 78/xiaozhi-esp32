wget https://zenodo.org/record/1227121/files/DKITCHEN_16k.zip

wget https://zenodo.org/record/1227121/files/DLIVING_16k.zip

wget https://zenodo.org/record/1227121/files/DWASHING_16k.zip

wget https://zenodo.org/record/1227121/files/NFIELD_16k.zip

wget https://zenodo.org/record/1227121/files/NPARK_16k.zip

wget https://zenodo.org/record/1227121/files/NRIVER_16k.zip

wget https://zenodo.org/record/1227121/files/OHALLWAY_16k.zip

wget https://zenodo.org/record/1227121/files/OMEETING_16k.zip

wget https://zenodo.org/record/1227121/files/OOFFICE_16k.zip

wget https://zenodo.org/record/1227121/files/PCAFETER_16k.zip

wget https://zenodo.org/record/1227121/files/PRESTO_16k.zip

wget https://zenodo.org/record/1227121/files/PSTATION_16k.zip

wget https://zenodo.org/record/1227121/files/TMETRO_16k.zip

wget https://zenodo.org/record/1227121/files/TCAR_16k.zip

wget https://zenodo.org/record/1227121/files/TBUS_16k.zip

wget https://zenodo.org/record/1227121/files/STRAFFIC_16k.zip

wget https://zenodo.org/record/1227121/files/SPSQUARE_16k.zip

unzip '*.zip'

mkdir -p ./combined_demand_channels/
for file in */*.wav; do
parentdir="$(dirname "$file")"
echo $parentdir
fname="$(basename "$file")"
cp $file ./combined_demand_channels/$parentdir+$fname
done
