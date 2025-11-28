#ifndef __GFXcanvasSerialDemo__
#define __GFXcanvasSerialDemo__
#include <Adafruit_GFX.h>

/**********************************************************************/
/*!
  @brief	Demonstrates using the GFXconvas classes as the backing store
  for a device driver.
*/
/**********************************************************************/
class GFXcanvas1SerialDemo : public GFXcanvas1 {
public:
  GFXcanvas1SerialDemo(uint16_t w, uint16_t h);

  /**********************************************************************/
  /*!
    @brief    Prints the current contents of the canvas to Serial
    @param    rotated  true to print according to the current GFX rotation,
    false to print to the native rotation of the canvas (or unrotated).
  */
  /**********************************************************************/
  void print(bool rotated);
};

/**********************************************************************/
/*!
  @brief	Demonstrates using the GFXconvas classes as the backing store
  for a device driver.
*/
/**********************************************************************/
class GFXcanvas8SerialDemo : public GFXcanvas8 {
public:
  GFXcanvas8SerialDemo(uint16_t w, uint16_t h);

  /**********************************************************************/
  /*!
    @brief    Prints the current contents of the canvas to Serial
    @param    rotated  true to print according to the current GFX rotation,
    false to print to the native rotation of the canvas (or unrotated).
  */
  /**********************************************************************/
  void print(bool rotated);
};

/**********************************************************************/
/*!
  @brief	Demonstrates using the GFXconvas classes as the backing store
  for a device driver.
*/
/**********************************************************************/
class GFXcanvas16SerialDemo : public GFXcanvas16 {
public:
  GFXcanvas16SerialDemo(uint16_t w, uint16_t h);

  /**********************************************************************/
  /*!
    @brief    Prints the current contents of the canvas to Serial
    @param    rotated  true to print according to the current GFX rotation,
    false to print to the native rotation of the canvas (or unrotated).
  */
  /**********************************************************************/
  void print(bool rotated);
};

#endif // __GFXcanvasSerialDemo__
