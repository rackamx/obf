/* Flattened by cflatten (C port) - control flow flattened into dispatcher loop */
#include <stdio.h>
#include <stdlib.h>
typedef int myint ;
struct Point { int x ; int y ; } ;
int fib ( int n )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        __cf_state += ((n <= 1)) ? (1) : (2) ;
        break ;
      }
      case 1:
      {
        return (n) ;
      }
      case 2:
      {
        return (fib ( n - 1 ) + fib ( n - 2 )) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int ptr_test ( )
{
  int v ;
  int * p ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        v = 42 ;
        p = & v ;
        * p = * p + 8 ;
        return (v) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

struct Point make_point ( int a , int b )
{
  struct Point pt ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        pt . x = a ;
        pt . y = b ;
        return (pt) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int struct_test ( )
{
  struct Point p ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        p = (struct Point) { 3 , 4 } ;
        return (p . x * p . x + p . y * p . y) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int switch_loop ( )
{
  int s ;
  int i ;
  __typeof__ ((i)) __flat_sw_1 ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        s = 0 ;
        i = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i < 5)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        __flat_sw_1 = (i) ;
        __cf_state += 4 ;
        break ;
      }
      case 3:
      {
        i ++ ;
        __cf_state += -2 ;
        break ;
      }
      case 4:
      {
        return (s) ;
      }
      case 5:
      {
        s += 100 ;
        __cf_state += -2 ;
        break ;
      }
      case 6:
      {
        __cf_state += (((__flat_sw_1) == (0))) ? (1) : (5) ;
        break ;
      }
      case 7:
      {
        __cf_state += 1 ;
        break ;
      }
      case 8:
      {
        s += 1 ;
        __cf_state += -3 ;
        break ;
      }
      case 9:
      {
        __cf_state += -6 ;
        break ;
      }
      case 10:
      {
        s += 10 ;
        __cf_state += -5 ;
        break ;
      }
      case 11:
      {
        __cf_state += (((__flat_sw_1) == (1))) ? (-3) : (1) ;
        break ;
      }
      case 12:
      {
        __cf_state += (((__flat_sw_1) == (2))) ? (-3) : (-2) ;
        break ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

myint typedef_test ( myint x )
{
  myint y ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        y = x * 2 ;
        return (y) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int comma_for ( )
{
  int s ;
  int i ;
  int j ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        s = 0 ;
        i = 0 , j = 10 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i < j)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        s ++ ;
        __cf_state += 1 ;
        break ;
      }
      case 3:
      {
        i ++ , j -- ;
        __cf_state += -2 ;
        break ;
      }
      case 4:
      {
        return (s) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int early ( )
{
  int i ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        i = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i < 100)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        __cf_state += ((i == 3)) ? (3) : (4) ;
        break ;
      }
      case 3:
      {
        i ++ ;
        __cf_state += -2 ;
        break ;
      }
      case 4:
      {
        return (- 1) ;
      }
      case 5:
      {
        return (i) ;
      }
      case 6:
      {
        __cf_state += -3 ;
        break ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int empty_loop ( int n )
{
  int i ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        i = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i < n)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        i ++ ;
        __cf_state += 1 ;
        break ;
      }
      case 3:
      {
        __cf_state += -2 ;
        break ;
      }
      case 4:
      {
        return (i) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int global_var = 7 ;
int use_global ( )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        global_var += 3 ;
        return (global_var) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int main ( )
{
  struct Point q ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        printf ( "fib10=%d (55)\n" , fib ( 10 ) ) ;
        printf ( "ptr=%d (50)\n" , ptr_test ( ) ) ;
        q = make_point ( 5 , 6 ) ;
        printf ( "pt=%d,%d\n" , q . x , q . y ) ;
        printf ( "struct=%d (25)\n" , struct_test ( ) ) ;
        printf ( "switch_loop=%d\n" , switch_loop ( ) ) ;
        printf ( "typedef=%d (14)\n" , typedef_test ( 7 ) ) ;
        printf ( "comma=%d (5)\n" , comma_for ( ) ) ;
        printf ( "early=%d (3)\n" , early ( ) ) ;
        printf ( "empty=%d (4)\n" , empty_loop ( 4 ) ) ;
        printf ( "global=%d (10)\n" , use_global ( ) ) ;
        printf ( "global=%d (13)\n" , use_global ( ) ) ;
        return (0) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

