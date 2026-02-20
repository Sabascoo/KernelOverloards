#include <stdio.h>

int* func() {
  	
  	// Creating static array
  	static int arr[2] = {10, 20};
    
  	// Returning multiple values using static array
  	return arr;
}
int main() {
  
    // Store the returened array
    int* arr = func();  
    printf("%d %d", arr[0], arr[1]);
    return 0;
}