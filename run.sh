# Compile the CPP program to a binary
g++ -std=c++11 -o main main.cpp

# Navigate to the provided directory
cd "$2"

# Loop through each file in the directory and execute it with the binary
for file in *.in
do
  if [ -r "$file" ]
  then
    ./../main "$1" "$file"
  fi
done

