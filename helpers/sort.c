/* Standalone historical sorting experiment; not used by engine move ordering. */
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

void insertionSort(int *t, int n){
    for(int i = 1; i < n; i++){
        int x = t[i];
        int j = i-1;
        while(j >= 0 && x < t[j]){
            t[j+1] = t[j];
            j--;
        }
        t[j+1] = x;
    }
}

void printArray(int *t, int i,int j){
    printf("[ ");
    for(int k = i; k <= j ; k++){
        printf("%d ",t[k]);
    }
    printf("]\n");
}

void quickSortRec(int *t, int i, int j){ //sorts t between i and j included
    if(i>=j) return;

    int m = (i+j)/2;
    int x = t[m];
    int copy[j-i+1];
    int left = 0;
    int right = j-i;
    for(int k = i; k < m; k++){
        int y = t[k];
        if(y <= x){
            copy[left] = y;
            left++;
        } else {
            copy[right] = y;
            right--;
        }
    }
    for(int k = m+1; k <= j; k++){
        int y = t[k];
        if(y <= x){
            copy[left] = y;
            left++;
        } else {
            copy[right] = y;
            right--;
        }
    }
    copy[left] = x;
    for(int k = 0; k <= j-i; k++){
        t[i+k] = copy[k];
    }
    quickSortRec(t,i,left-1);
    quickSortRec(t,left+1,j);
}

void quickSort(int *t, int n){
    quickSortRec(t,0,n-1);
}

bool testSorted(int *original, int *t, int n){

    printf("[ ");
    for(int i = 0; i<n ; i++){
        printf("%d ",t[i]);
    }
    printf("]\n\n\n");
    for(int i = 0; i < n-1; i++){
        if(t[i]>t[i+1]) return false;
    }
    bool seen[n];
    for(int i=0; i < n;i++){
        seen[i] = false;
    }
    for(int i = 0; i < n; i++){
        bool res = false;
        for(int j = 0; j < n; j++){
            if(original[i] == t[j] && !seen[j]){
                seen[j] = true;
                res = true;
                break;
            }
        }
        if(!res) return false;
    }
    return true;
}

int main(){
    srand((unsigned int)time(NULL));
    for(int i = 5; i < 25; i++){
        int S = 1000;
        int** data1 = malloc(S*sizeof(int*));
        int** data2 = malloc(S*sizeof(int*));
        for(int s = 0 ; s < S; s++){
            data1[s] = malloc(i*sizeof(int));
            data2[s] = malloc(i*sizeof(int));
            for(int k = 0; k < i; k++){
                data1[s][k] = rand();
                data2[s][k] = data1[s][k];
            }
        }

        for(int s = 0; s < S; s++){
            quickSort(data2[s],i);
            bool b = testSorted(data1[s],data2[s],i);
            if(!b){
                printArray(data1[s],0,i-1);
                printArray(data2[s],0,i-1);
                assert(false);
            }
        }
    }
    return 0;
}
