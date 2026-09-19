/* Flattened by cflatten (C port) - control flow flattened into dispatcher loop */
#include <stdio.h>
int proto ( int x ) ;
int empty_func ( )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        return (42) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

void void_empty ( )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        printf ( "void\n" ) ;
        __cf_state += -1 ;
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

int switch_no_default ( int x )
{
  int r ;
  __typeof__ ((x)) __flat_sw_1 ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        r = 0 ;
        __flat_sw_1 = (x) ;
        __cf_state += 2 ;
        break ;
      }
      case 1:
      {
        return (r) ;
      }
      case 2:
      {
        __cf_state += (((__flat_sw_1) == (1))) ? (1) : (3) ;
        break ;
      }
      case 3:
      {
        r = 10 ;
        __cf_state += -2 ;
        break ;
      }
      case 4:
      {
        r = 20 ;
        __cf_state += -3 ;
        break ;
      }
      case 5:
      {
        __cf_state += (((__flat_sw_1) == (2))) ? (-1) : (-4) ;
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

int braces_in_string ( )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        printf ( "{ not a brace } (also ; semicolon)\n" ) ;
        printf ( "quote \" still string %d\n" , 7 ) ;
        return (7) ;
      }
      default:
      {
        __cf_state += -1 - __cf_state ;
        break ;
      }
    }
  }
}

int proto ( int x )
{
  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        return (x * 3) ;
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
        printf ( "%d\n" , empty_func ( ) ) ;
        void_empty ( ) ;
        printf ( "%d %d %d\n" , switch_no_default ( 1 ) , switch_no_default ( 2 ) , switch_no_default ( 9 ) ) ;
        printf ( "%d\n" , braces_in_string ( ) ) ;
        printf ( "%d\n" , proto ( 5 ) ) ;
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

