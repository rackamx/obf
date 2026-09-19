/* Flattened by cflatten (C port) - control flow flattened into dispatcher loop */
#include <stdio.h>
int classify ( int x )
{
  __typeof__ ((x)) __flat_sw_1 ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        __flat_sw_1 = (x) ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += (((__flat_sw_1) == (1))) ? (1) : (5) ;
        break ;
      }
      case 2:
      {
        return (10) ;
      }
      case 3:
      {
        __cf_state += 1 ;
        break ;
      }
      case 4:
      {
        return (20) ;
      }
      case 5:
      {
        return (30) ;
      }
      case 6:
      {
        __cf_state += (((__flat_sw_1) == (2))) ? (-3) : (1) ;
        break ;
      }
      case 7:
      {
        __cf_state += (((__flat_sw_1) == (3))) ? (-3) : (-2) ;
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

int loop_break ( )
{
  int s ;
  int i ;

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
        __cf_state += ((i < 10)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        __cf_state += ((i == 5)) ? (3) : (4) ;
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
        __cf_state += -1 ;
        break ;
      }
      case 6:
      {
        s += i ;
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

int do_while_test ( int n )
{
  int i ;
  int s ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        i = 0 ;
        s = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        s += i ;
        i ++ ;
        __cf_state += 1 ;
        break ;
      }
      case 2:
      {
        __cf_state += ((i < n)) ? (-1) : (1) ;
        break ;
      }
      case 3:
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

int goto_test ( int x )
{
  int y ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        y = 0 ;
        __cf_state += ((x == 0)) ? (1) : (2) ;
        break ;
      }
      case 1:
      {
        __cf_state += 2 ;
        break ;
      }
      case 2:
      {
        y = x * 2 ;
        __cf_state += 2 ;
        break ;
      }
      case 3:
      {
        y += 1 ;
        return (y) ;
      }
      case 4:
      {
        y += 100 ;
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

int nested ( int n )
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
        j = 0 ;
        __cf_state += 3 ;
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
        __cf_state += ((j < n)) ? (1) : (3) ;
        break ;
      }
      case 6:
      {
        __cf_state += ((( i + j ) % 2 == 0)) ? (3) : (4) ;
        break ;
      }
      case 7:
      {
        j ++ ;
        __cf_state += -2 ;
        break ;
      }
      case 8:
      {
        __cf_state += -5 ;
        break ;
      }
      case 9:
      {
        __cf_state += -2 ;
        break ;
      }
      case 10:
      {
        s += i * j ;
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

int shadow ( )
{
  int x ;
  int r ;
  int x__h1 ;
  int x__h2 ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        x = 1 ;
        r = 0 ;
        x__h1 = 2 ;
        r += x__h1 ;
        x__h2 = 3 ;
        r += x__h2 ;
        r += x__h1 ;
        r += x ;
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

int array_test ( )
{
  int a [ 3 ] ;
  int s ;
  int i ;
  char msg [3] ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        { static const __typeof__ ( a [0]) __flat_init_1 [] = { 1 , 2 , 3 } ; __builtin_memset ( a , 0 , sizeof ( a ) ) ; __builtin_memcpy ( a , __flat_init_1 , sizeof ( __flat_init_1 ) < sizeof ( a ) ? sizeof ( __flat_init_1 ) : sizeof ( a ) ) ; }
        s = 0 ;
        i = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((i < 3)) ? (1) : (3) ;
        break ;
      }
      case 2:
      {
        s += a [ i ] ;
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
        __builtin_memset ( msg , 0 , sizeof ( msg ) ) ;
        __builtin_memcpy ( msg , "hi" , sizeof ( "hi" ) < sizeof ( msg ) ? sizeof ( "hi" ) : sizeof ( msg ) ) ;
        s += msg [ 0 ] + msg [ 1 ] ;
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

int main ( )
{
  int k ;

  int __cf_state = 0 ;
  while (__cf_state != -1)
  {
    switch (__cf_state)
    {
      case 0:
      {
        printf ( "classify 1=%d (10)\n" , classify ( 1 ) ) ;
        printf ( "classify 2=%d (20)\n" , classify ( 2 ) ) ;
        printf ( "classify 3=%d (20)\n" , classify ( 3 ) ) ;
        printf ( "classify 9=%d (30)\n" , classify ( 9 ) ) ;
        printf ( "loop_break=%d (10)\n" , loop_break ( ) ) ;
        printf ( "do_while=%d (6)\n" , do_while_test ( 4 ) ) ;
        printf ( "goto0=%d (1)\n" , goto_test ( 0 ) ) ;
        printf ( "goto5=%d (110)\n" , goto_test ( 5 ) ) ;
        printf ( "nested=%d\n" , nested ( 4 ) ) ;
        printf ( "shadow=%d (8)\n" , shadow ( ) ) ;
        printf ( "array=%d (215)\n" , array_test ( ) ) ;
        k = 0 ;
        __cf_state += 1 ;
        break ;
      }
      case 1:
      {
        __cf_state += ((k < 3)) ? (1) : (2) ;
        break ;
      }
      case 2:
      {
        k ++ ;
        __cf_state += -1 ;
        break ;
      }
      case 3:
      {
        printf ( "k=%d (3)\n" , k ) ;
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

