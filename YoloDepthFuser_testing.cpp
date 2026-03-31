// you can tell that Annabeth doesn't use C++. or know C.

#include <iostream>
#include <queue>
#include <array>
#include <algorithm>

void print_queue(std::queue<int> q)
{
  while (!q.empty())
  {
    std::cout << q.front() << " ";
    q.pop();
  }
  std::cout << std::endl;
}

void print_array(std::array<int, 4> a){
    for (int element : a) {
        std::cout << element << " ";
    }
    std::cout << std::endl;
}

// template <typename T> void add_to_limited_queue(std::queue<T>* q, T n){
//     (*q).push(n);
//     if ((*q).size() > 3){
//         q->pop();
//     }
// }

template <typename T> void add_to_limited_array(std::array<T, 4>& a, T n){
    a[3] = n;
    std::rotate(a.begin(), a.begin() + 1, a.end());
    a[3] = NULL;
}

int main() {
    std::array<int, 4> detbuf;
    detbuf.fill(NULL);
    add_to_limited_array(detbuf, 1);
    print_array(detbuf);
    
    add_to_limited_array(detbuf, 2);
    print_array(detbuf);
    
    add_to_limited_array(detbuf, 3);
    print_array(detbuf);
    
    add_to_limited_array(detbuf, 4);
    print_array(detbuf);
    
    return 0;
}