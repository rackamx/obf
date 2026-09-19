/* Flattened by cflatten (C port) - control flow flattened into dispatcher loop */
#include <stdio.h>
#include <string.h>
int main ( int argc , char * * argv )
{
  int total ;
  int i ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        total = 0 ;
        i = 1 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i < argc)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        printf ( "arg%d=%s\n" , i , argv [ i ] ) ;
        total += strlen ( argv [ i ] ) ;
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
        printf ( "total=%d\n" , total ) ;
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

