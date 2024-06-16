#include <User_Setup.h>

#include <SPI.h>
#include <TFT_eSPI.h>

//TFT COLORS
#define ILI9341_BLACK       0x0000  ///<   0,   0,   0
#define ILI9341_NAVY        0x000F  ///<   0,   0, 123
#define ILI9341_DARKGREEN   0x03E0  ///<   0, 125,   0
#define ILI9341_DARKCYAN    0x03EF  ///<   0, 125, 123
#define ILI9341_MAROON      0x7800  ///< 123,   0,   0
#define ILI9341_PURPLE      0x780F  ///< 123,   0, 123
#define ILI9341_OLIVE       0x7BE0  ///< 123, 125,   0
#define ILI9341_LIGHTGREY   0xC618  ///< 198, 195, 198
#define ILI9341_DARKGREY    0x7BEF  ///< 123, 125, 123
#define ILI9341_BLUE        0x001F  ///<   0,   0, 255
#define ILI9341_GREEN       0x07E0  ///<   0, 255,   0
#define ILI9341_CYAN        0x07FF  ///<   0, 255, 255
#define ILI9341_RED         0xF800  ///< 255,   0,   0
#define ILI9341_MAGENTA     0xF81F  ///< 255,   0, 255
#define ILI9341_YELLOW      0xFFE0  ///< 255, 255,   0
#define ILI9341_WHITE       0xFFFF  ///< 255, 255, 255
#define ILI9341_ORANGE      0xFD20  ///< 255, 165,   0
#define ILI9341_GREENYELLOW 0xAFE5  ///< 173, 255,  41
#define ILI9341_PINK        0xFC18  ///< 255, 130, 198


//Graph values
#define VALUE_MIN 0
#define VALUE_LOW 100
#define VALUE_HIGH 200
#define VALUE_MAX 300

#define TEXT_SIZE 5


//TFT init
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
TFT_eSPI tft = TFT_eSPI();


//declarated vars for screen
int third_width;
int quart_height;
int half_height;


//arrays vars
int array_index = 0;

#define array_size 308 // half of width of screen - 2. Curren is for 4 inches

int arr_1[array_size];
int arr_2[array_size];

//graph flag
bool graph_flag_clear = true;


//text vars
#define TEXT_ARRAY_SIZE 3
float text_values_1[TEXT_ARRAY_SIZE];
float text_values_2[TEXT_ARRAY_SIZE];
float avarage_values_1 = 0;
float avarage_values_2 = 0;
float text_temp_val_1;
float text_temp_val_2;

//MAIN FUNCTIONS

void configure(){
  //tft.begin();
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  half_height = tft.height() / 2;
  quart_height = tft.height() / 4;
  third_width =  tft.width() / 3;
}


//Service dash line function for graph
void drawDashedLine(int x0, int y0, int x1, int y1, uint16_t color, int dashLength, int gapLength) {
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int length = max(dx, dy);

  for (int i = 0; i < length; i += (dashLength + gapLength)) {
    int xStart = x0 + (i * (x1 - x0) / length);
    int yStart = y0 + (i * (y1 - y0) / length);
    int xEnd = x0 + ((i + dashLength) * (x1 - x0) / length);
    int yEnd = y0 + ((i + dashLength) * (y1 - y0) / length);

    tft.drawLine(xStart, yStart, xEnd, yEnd, color);
  }
}


//Axis function for graph
void set_axis(){
  // first axis
  tft.drawLine(third_width, 1, third_width, half_height - 5, ILI9341_WHITE);
  tft.drawLine(third_width, half_height - 5, tft.width() - 1, half_height - 5, ILI9341_WHITE);
  drawDashedLine(third_width, (half_height / 3), tft.width() - 1, (half_height / 3), ILI9341_YELLOW, 5, 3);
  drawDashedLine(third_width, 2 * (half_height / 3), tft.width() - 1, 2 * (half_height / 3), ILI9341_YELLOW, 5, 3);
  

  // second axis
  tft.drawLine(third_width, tft.height() / 2 + 5, third_width, tft.height() - 1, ILI9341_WHITE);
  tft.drawLine(third_width, tft.height() - 1, tft.width() - 1, tft.height() - 1, ILI9341_WHITE);
  drawDashedLine(third_width, half_height + (half_height / 3), tft.width() - 1, half_height + (half_height / 3), ILI9341_YELLOW, 5, 3);
  drawDashedLine(third_width, half_height +  2 * (half_height / 3), tft.width() - 1, half_height + 2 * (half_height / 3), ILI9341_YELLOW, 5, 3);
}

//Service function for clearing text
void clear_text(){
  tft.fillRect(0, 0, third_width, tft.height(), ILI9341_BLACK);
}

//Text function for values aside graph
void set_text(float val_1, float val_2){

  //TODO: set text color depending on min max values

  clear_text();
  // first num
  tft.setCursor(1, quart_height);
  if(val_1 * 100 >= VALUE_LOW && val_1 * 100 <= VALUE_HIGH) tft.setTextColor(ILI9341_GREEN);
  else tft.setTextColor(ILI9341_RED);
  tft.setTextSize(TEXT_SIZE);
  tft.print(val_1, 2);

  // second num
  tft.setCursor(1, quart_height * 3);
  if(val_2 * 100 >= VALUE_LOW && val_2 * 100 <= VALUE_HIGH) tft.setTextColor(ILI9341_GREEN);
  else tft.setTextColor(ILI9341_RED);
  tft.setTextSize(TEXT_SIZE);
  tft.print(val_2, 2);
}

void text_avarage(){
  avarage_values_1 = 0;
  avarage_values_2 = 0;

  for(int i = 0; i < TEXT_ARRAY_SIZE; i ++) {
    avarage_values_1 += text_values_1[i];
    avarage_values_2 += text_values_2[i];
  }

  avarage_values_1 /= 3;
  avarage_values_2 /= 3;
  
  set_text(avarage_values_2, avarage_values_1);
}

void add_text_value(float val_1, float val_2){
  for(int i = TEXT_ARRAY_SIZE - 1; i > 0; i --) {
    if (i == TEXT_ARRAY_SIZE - 1) {
      text_temp_val_1 = text_values_1[i];
      text_temp_val_2 = text_values_2[i];
      text_values_1[i] = val_1;
      text_values_2[i] = val_2;
    }
    else {
      text_values_1[i - 1] = text_values_1[i];
      text_values_2[i - 1] = text_values_2[i];
      text_values_1[i] = text_temp_val_1;
      text_values_2[i] = text_temp_val_2;
      text_temp_val_1 = text_values_1[i - 1];
      text_temp_val_2 = text_values_2[i - 1];
    }
  }
  text_avarage();
}

//Service function to set point at graph
//map function converts y value to standart Cartesian grid
//that grid depends on GRAPH NOT ON SCREEN
//so i use abs function to convert y coor of point to y coor of screen
void set_point(bool switch_arr, bool clear, int value_index, int value){
  int x = third_width + 1 + value_index;
  int y;
  if (value > VALUE_MAX) y = half_height - 10;
  else{
    if(value < VALUE_MIN) y = 10;
    else y = map(value, VALUE_MIN, VALUE_MAX, 10, half_height - 10);
  }
  y = abs(y - half_height);
  if (!switch_arr) y += half_height;
  if (clear) tft.drawPixel(x, y, ILI9341_BLACK);
  else tft.drawPixel(x, y, ILI9341_ORANGE);
  /*
  Serial.print(" switch: ");
  Serial.print(switch_arr);
  Serial.print(", value: ");
  Serial.print(value);
  Serial.print(", x: ");
  Serial.print(x);
  Serial.print(", y: ");
  Serial.println(y);
  */
}


//Function to set graph
void set_graph(){
  for(int i = 0; i < array_index; i ++){
    set_point(false, false, i, arr_1[i]);
    set_point(true, false, i, arr_2[i]);
  }
  graph_flag_clear = false;
}


//Function to clear graph
void clear_graph(){
  for(int i = 0; i < array_index; i ++){
    set_point(false, true, i, arr_1[i]);
    set_point(true, true, i, arr_2[i]);
  }
  drawDashedLine(third_width, (half_height / 3), tft.width() - 1, (half_height / 3), ILI9341_YELLOW, 5, 3);
  drawDashedLine(third_width, 2 * (half_height / 3), tft.width() - 1, 2 * (half_height / 3), ILI9341_YELLOW, 5, 3);

  drawDashedLine(third_width, half_height + (half_height / 3), tft.width() - 1, half_height + (half_height / 3), ILI9341_YELLOW, 5, 3);
  drawDashedLine(third_width, half_height +  2 * (half_height / 3), tft.width() - 1, half_height + 2 * (half_height / 3), ILI9341_YELLOW, 5, 3);
  graph_flag_clear = true;
}

void get_screen(){
  tft.setCursor(10, 10, 2);
  tft.print("Height: ");
  tft.print(tft.height());
  tft.println();
  tft.print("Width: ");
  tft.print(tft.width());
  tft.println();
}


//Service function for adding values to array
//Also shift array to left for easy setting graph
void add_value(float value1, float value2){
  if(array_index == array_size - 1){
    if(!graph_flag_clear) clear_graph();
    memcpy(arr_1, arr_1 + 1 , (array_size - 1) * sizeof(int));
    arr_1[array_size - 1] = int(value1 * 100);

    memcpy(arr_2, arr_2 + 1 , (array_size - 1) * sizeof(int));
    arr_2[array_size - 1] = int(value2 * 100);
  }
  else{
    arr_1[array_index] = int(value1 * 100);
    arr_2[array_index] = int(value2 * 100);
  }
  
  if(array_index < array_size - 1) array_index++;
}
