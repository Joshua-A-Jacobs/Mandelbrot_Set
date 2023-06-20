/*
The program creates a Mandelbrot Set and displays it to the screen. You can navigate with W,A,S, and D and zoom in/out with E and Q. 
Create a Bitmap file of what you're viewing with the ~ button.
*/


#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<pthread.h>
#include<ncurses.h>

typedef struct complex_t{
    long double real;
    long double imagine;
} complex_t;

typedef struct window_t{
    double minX;
    double maxX;
    double minY;
    double maxY;
    double width;
    double height;
    double scale;
} window_t; 

typedef struct threadArgs {
    FILE* bmp;
    window_t window;
    complex_t x;
    int index;
    int* resultArray;
} threadArgs;

//prototypes
int is_in_set(complex_t);
complex_t complex_multiply(complex_t,complex_t);
complex_t complex_add(complex_t,complex_t);
complex_t complex_sub(complex_t,complex_t);
int complex_magnitude(complex_t);
complex_t scale(window_t, int, int);
void create_window(window_t,WINDOW*);
void make_bitmap(window_t);
void BMPheader(FILE*);
void pixelGen(FILE*,window_t);
double getMin(double, double);
void* addPixelsToImage(void*);
void* setColumnsForWindow(void*);

const int imageSimPrecision = 250;
const int imageGenResolution = 5000;
//hex2 concatanated with hex1 must = imageGenPrecision
//0x1388 = 5000
const int imageGenSizeHex1 = 0x88;
const int imageGenSizeHex2 = 0x13;
const int imageGenAccuracy = 500;
const int divergenceSpeedMax = 3;

int main(void){
    //initilizations
    WINDOW *win;
    char c;
    window_t window;
    double width,height,min;

    initscr(); //initilizes the window
    
    cbreak();
    noecho();   //don't display user input
    win = newwin(LINES,COLS,0,0);//creates the window to be the size of the screen
    //adjusts the window according to the screen size
    width = COLS-2;
    height = LINES;
    min = getMin(width,height);
    window.scale = 1/(min)*2;
    window.maxY= height*window.scale/2;
    window.minY= -window.maxY;
    window.height= window.maxY*2;
    window.maxX= width*window.scale/2;
    window.minX= -window.maxX;
    window.width = window.maxX*2;

    start_color();  //allows colors
    init_pair(1,COLOR_BLUE,COLOR_BLUE); //creates a pair to be completely blue
    create_window(window,win);  //creates the Mendelbrot set onscreen
    while(1){
        c = wgetch(win);    //gets input from user
        switch(c){
            
            //adjusts the scale based on the direction the user wishes to go
            case 'w':
                window.minY -=window.scale;
                window.maxY -=window.scale;
                break; 
            case 'd':
                window.minX +=window.scale;
                window.maxX +=window.scale;
                break;
            case 's':
                window.minY +=window.scale;
                window.maxY +=window.scale;
                break;
            case 'a':
                window.minX -=window.scale;
                window.maxX -=window.scale;
                break;

            //adjusts the window and scale for the zoom in/out
            case 'e':
                window.scale /= 2;
                window.height /=2;
                window.width /=2;
                window.maxX = window.maxX - window.width/2;  
                window.minX = window.minX + window.width/2;
                window.maxY = window.maxY - window.height/2;
                window.minY = window.minY + window.height/2;;
                break;
            case 'q':
                window.scale *= 2;
                window.height *= 2;
                window.width *= 2;
                window.maxX = window.maxX + window.width/4;
                window.minX = window.minX - window.width/4;
                window.maxY = window.maxY + window.height/4;
                window.minY = window.minY - window.height/4;;
                break;

            //creates a bitmap if the user pushes ` or ~
            case '`':
            case '~':
                wattroff(win, COLOR_PAIR(1));
                wclear(win);
                wprintw(win,"Loading...");
                wrefresh(win);
                make_bitmap(window);
                goto end;

            //quits the window if the user presses a non-specified key
            default:
                goto end;
            }
        wclear(win);//clears the window to make room for the new one
        create_window(window,win);  //creates the Mendelbrot set onscreen
        wrefresh(win);  //displays the set
    }
end:
endwin();//closes the window and exits
return 0;
}

double getMin(double x, double y){
    if(x >= y){
        return y;}
    else{
        return x;}
    }

int is_in_set(complex_t x){
    //initilizations
    int i,mag;
    complex_t z;
    z.real = 0;
    z.imagine = 0;

    for(i = 0;i <= imageSimPrecision; i++){
        z = complex_multiply(z,z);//squares z and then adds x
        z = complex_add(z,x);
        mag = complex_magnitude(z); //gets the magnitude
        //this checks to see if it is growing too fast, thus diverging
        if(mag > divergenceSpeedMax){
            return 0;}//diverges
    }
return 1;//converges
}

complex_t complex_multiply(complex_t a,complex_t b){
    complex_t x; //creates the product

    //complex multiplication
    x.real = a.real*b.real - a.imagine*b.imagine;
    x.imagine = a.real * b.imagine + a.imagine*b.real;
    return x; //returns the product
}

complex_t complex_add(complex_t a,complex_t b){
    complex_t x;//creates the sum
    
    //complex addition
    x.real = a.real + b.real;
    x.imagine = a.imagine + b.imagine;
    return x;//returns the sum

}

complex_t complex_sub(complex_t a,complex_t b){
    complex_t x;//creates the difference

    //complex subtraction
    x.real = a.real - b.real;
    x.imagine = a.imagine - b.imagine;
    return x;//returns the difference

}

int complex_magnitude(complex_t x){
    //initlization
    int mag;
    double real,imagine,temp;

    //gets the magnitude of the complex number: sqrt(real^2+imaginary^2)
    real = x.real*x.real;
    imagine = x.imagine*x.imagine;
    temp = real+imagine;
    mag = sqrt(temp);
    return mag;//returns the magnitude
}

void create_window(window_t window,WINDOW *win){
    //initilization
    int i, j;
    complex_t x;

    //Thread declaration
    pthread_t threads[LINES];
    threadArgs args[LINES];

    for(i=0; i < LINES; i++){//imaginary loop
        //i = round(i*1000000000000)/1000000000000;//gets rid of any garbage on the end of double
        x.imagine = window.minY + window.scale*i;

        //sets the multithreads arguments
        args[i].window = window;
        args[i].x = x;
        //Initilizes an array of the required size to handle the loop of diverging integers
        args[i].resultArray =  malloc((COLS-1) * sizeof(int));

        if (pthread_create(&threads[i], NULL, setColumnsForWindow, &args[i]))
        {
            printf("ERROR ON CREATE THREAD #%d", i);
        }
    }
    for(i=0; i < LINES; i++){
        if(pthread_join(threads[i], NULL))
        {
            printf("ERROR ON JOIN THREAD #%d", i);
        }
    }
    for(i=0; i < LINES; i++){
        for(j=0; j < COLS-1; j++){
            if(args[i].resultArray[j])
            {
                wattroff(win, COLOR_PAIR(1));//sets to default
            }
            else
            {
                wattron(win, COLOR_PAIR(1));//sets to blue
            }
            wprintw(win," ");
        }
        wprintw(win,"\n");
    }
}

void* setColumnsForWindow(void* tArgs)
{
    threadArgs *args = (threadArgs*) tArgs;
    window_t window= args->window;
    complex_t x=args->x;
    
    //real loop
    for(int i = 0; i < COLS-1; i++ ){
            x.real = window.minX + window.scale*i;
            args->resultArray[i] = is_in_set(x);
        }
}

void make_bitmap(window_t window){
    FILE *bmp;
    bmp = fopen("b.bmp","wb+");
    BMPheader(bmp);
    pixelGen(bmp,window);
    fclose(bmp);
}    

void BMPheader(FILE *file)
{
    int i;
    // -- FILE HEADER -- //

    // bitmap signature
    fputc('B',file);
    fputc('M',file);

    // file size
    fputc(0x10,file);
    fputc(0x10,file);
    fputc(0x10,file);
    fputc(0xF,file);

    // reserved field (in hex. 00 00 00 00)
    for( i = 6; i < 10; i++) fputc(0,file);;

    // offset of pixel data inside the image
    //The offset, i.e. starting address, of the byte where the bitmap image data (pixel array) can be found.
    //here 1078
    fputc(0x36,file);
    fputc(0x00,file);
    fputc(0,file);
    fputc(0,file);


    // -- BITMAP HEADER -- //

    // header size
    fputc(0x28,file);
    for( i = 15; i < 18; i++) fputc(0,file);

    // width of the image
    fputc(imageGenSizeHex1,file);
    fputc(imageGenSizeHex2,file);
    fputc(0,file);
    fputc(0,file);

    // height of the image
    fputc(imageGenSizeHex1,file);
    fputc(imageGenSizeHex2,file);
    fputc(0,file);
    fputc(0,file);

    // no of color planes, must be 1
    fputc(1,file);
    fputc(0,file);

    // number of bits per pixel
    fputc(0x18,file); // 1 byte
    fputc(0,file);

    // compression method (no compression here)
    for( i = 30; i < 34; i++) fputc(0,file);;

    // size of pixel data
    fputc(0x10,file);; // 400 bytes => 400 pixels ,,,, 20x20x1
    fputc(0,file);
    fputc(0,file);
    fputc(0,file);

    // horizontal resolution of the image - pixels per meter (2835)
    fputc(0x00,file);
    fputc(0xb0,file);
    fputc(0,file);
    fputc(0,file);

    // vertical resolution of the image - pixels per meter (2835)
    fputc(0x00,file);
    fputc(0xb0,file);
    fputc(0,file);
    fputc(0,file);

    // color palette information here 256
    fputc(0,file);
    fputc(0,file);
    for( i = 48; i < 50; i++) fputc(0,file);

    // number of important colors
    // if 0 then all colors are important
    for( i = 50; i < 54; i++) fputc(0,file);
}


void pixelGen(FILE *bmp,window_t window){
    //initilization
    double halfHeight;
    int i,j;
    complex_t x;
    
    //TODO:Not sure what's going on here, review later
    window.scale = window.width/imageGenResolution;
    halfHeight = (window.width - window.height)/2;
    window.minY = window.minY - halfHeight;
    window.maxY = window.maxY + halfHeight;

    //Total number of loops needed
    int pixelCountY = round((window.maxY - window.minY) / window.scale);
    //Thread declaration
    pthread_t threads[pixelCountY];
    threadArgs args[pixelCountY];

    x.imagine = window.maxY;
    for(i = 0; i < pixelCountY; i++){//imaginary loop
        
        //sets the multithreads arguments
        args[i].bmp = bmp;
        args[i].window = window;
        args[i].x = x;
        //Initilizes an array of the required size to handle the loop of diverging integers
        args[i].resultArray =  malloc(imageGenResolution * sizeof(int));

        //creates the thread that will output an array of diverging integers
        if (pthread_create(&threads[i], NULL, addPixelsToImage, &args[i]))
        {
            printf("ERROR ON CREATE THREAD #%d", i);
        }
        
        x.imagine -= window.scale;
    }

    //Now joins the threads
    for(i = 0; i < pixelCountY; i++)
    {
        if(pthread_join(threads[i], NULL))
        {
            printf("ERROR ON JOIN THREAD #%d", i);
        }
    }   

    //Finally, takes the resulting array and puts it together into a bitmap image
    for(i = 0; i < pixelCountY; i++)
    {
        for(j = 0; j < imageGenResolution;j++)
        {
            fputc(args[i].resultArray[j], bmp);
            fputc(0, bmp);
            fputc(0, bmp);
        }
    }
}

void *addPixelsToImage(void* tArgs)
{
    threadArgs *args = (threadArgs*) tArgs;
    window_t window= args->window;
    complex_t x=args->x;
    FILE* bmp=args->bmp;
    int index = args->index;

    double j = window.minX;
    int mag,divergenceSpeed,l,k;
    complex_t z;
    

    for (l = 0; l < imageGenResolution; l++) // real loop
    { 
        x.real = j;
        z.real = 0;
        z.imagine = 0;
        divergenceSpeed = 0;

        for (k = 0; k <= imageGenAccuracy; k++)
        {
            z = complex_multiply(z, z); // squares z and then adds x
            z = complex_add(z, x);

            mag = complex_magnitude(z); // calculates the magnitude

            // this checks to see if the number is growing too fast, thus diverging
            if (mag > divergenceSpeedMax)
            {
                divergenceSpeed = k;
                break;
            }
        }
        j += window.scale;
        args->resultArray[l] = divergenceSpeed;
    }
}
