/* Flattened by cflatten (C port) - control flow flattened into dispatcher loop */
#include <stdio.h>
int counter ( )
{
  static int n = 0 ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        n ++ ;
        return (n) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int const_test ( )
{
  int x ;
  int y ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        x = 5 ;
        y = x + 2 ;
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

int main ( )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        printf ( "%d\n" , counter ( ) ) ;
        printf ( "%d\n" , counter ( ) ) ;
        printf ( "%d\n" , counter ( ) ) ;
        printf ( "const=%d (7)\n" , const_test ( ) ) ;
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

