/* Flattened by cflatten (C port) - control flow flattened into dispatcher loop */
#include <stdio.h>
int fact ( int n )
{
  int r ;
  int i ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        r = 1 ;
        i = 2 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i <= n)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        r = r * i ;
        __cf_state += 1 ;
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
        return (r) ;
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
  int i ;
  int x ;
  int s ;
  int j ;

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
        __cf_state += ((i <= 6)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        printf ( "%d! = %d\n" , i , fact ( i ) ) ;
        __cf_state += 1 ;
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
        x = 10 ;
        __cf_state += ((x > 5)) ? (1) : (2) ;
        break ;
      }
      case 5:
      {
        printf ( "big\n" ) ;
        __cf_state += 2 ;
        break ;
      }
      case 6:
      {
        printf ( "small\n" ) ;
        __cf_state += 1 ;
        break ;
      }
      case 7:
      {
        s = 0 ;
        j = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 8:
      {
        __cf_state += ((j < 5)) ? (1) : (2) ;
        break ;
      }
      case 9:
      {
        __cf_state += ((j == 3)) ? (2) : (3) ;
        break ;
      }
      case 10:
      {
        printf ( "s=%d\n" , s ) ;
        return (0) ;
      }
      case 11:
      {
        j ++ ;
        __cf_state += -3 ;
        break ;
      }
      case 12:
      {
        s += j ;
        j ++ ;
        __cf_state += -4 ;
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

