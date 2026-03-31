/*
 * Avalon memory-mapped peripheral that generates VGA
 *
 * Stephen A. Edwards
 * Columbia University
 *
 * Register map:
 *
 * Byte Offset  7 ... 0   Meaning
 *        0    | X[7:0] | Ball X coordinate, low byte
 *        1    | X[9:8] | Ball X coordinate, high bits
 *        2    | Y[7:0] | Ball Y coordinate, low byte
 *        3    | Y[9:8] | Ball Y coordinate, high bits
 */

module vga_ball(input logic        clk,
                input logic        reset,
                input logic [7:0]  writedata,
                input logic        write,
                input logic        chipselect,
                input logic [2:0]  address,

                output logic [7:0] VGA_R, VGA_G, VGA_B,
                output logic       VGA_CLK, VGA_HS, VGA_VS,
                                   VGA_BLANK_n,
                output logic       VGA_SYNC_n);

   logic [10:0]    hcount;
   logic [9:0]     vcount;

   localparam int BALL_RADIUS = 16;
   localparam logic [23:0] BALL_RADIUS_SQ = 24'd256;

   logic [9:0]     ball_x_pending, ball_y_pending;
   logic [9:0]     ball_x_active,  ball_y_active;
   logic [9:0]     pixel_x;
   logic [9:0]     pixel_y;
   logic signed [11:0] dx, dy;
   logic [23:0]    dist2;
   logic           ball_on;
        
   vga_counters counters(.clk50(clk), .*);

   always_ff @(posedge clk)
     if (reset) begin
        ball_x_pending <= 10'd320;
        ball_y_pending <= 10'd240;
        ball_x_active  <= 10'd320;
        ball_y_active  <= 10'd240;
     end else begin
        if (chipselect && write)
          case (address)
            3'h0 : ball_x_pending[7:0] <= writedata;
            3'h1 : ball_x_pending[9:8] <= writedata[1:0];
            3'h2 : ball_y_pending[7:0] <= writedata;
            3'h3 : ball_y_pending[9:8] <= writedata[1:0];
            default : ;
          endcase

        // Latch a new position once per frame during vertical blanking
        // so the ball does not tear across visible lines.
        if (hcount == 11'd0 && vcount == 10'd480) begin
           ball_x_active <= ball_x_pending;
           ball_y_active <= ball_y_pending;
        end
     end

   assign pixel_x = hcount[10:1];
   assign pixel_y = vcount;
   assign dx = $signed({1'b0, pixel_x}) - $signed({1'b0, ball_x_active});
   assign dy = $signed({1'b0, pixel_y}) - $signed({1'b0, ball_y_active});
   assign dist2 = dx * dx + dy * dy;
   assign ball_on = (dist2 <= BALL_RADIUS_SQ);

   always_comb begin
      {VGA_R, VGA_G, VGA_B} = {8'h0, 8'h0, 8'h0};
      if (VGA_BLANK_n)
        if (ball_on)
          {VGA_R, VGA_G, VGA_B} = {8'hff, 8'hff, 8'hff};
   end
               
endmodule

module vga_counters(
 input logic         clk50, reset,
 output logic [10:0] hcount,  // hcount[10:1] is pixel column
 output logic [9:0]  vcount,  // vcount[9:0] is pixel row
 output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n);

/*
 * 640 X 480 VGA timing for a 50 MHz clock: one pixel every other cycle
 * 
 * HCOUNT 1599 0             1279       1599 0
 *             _______________              ________
 * ___________|    Video      |____________|  Video
 * 
 * 
 * |SYNC| BP |<-- HACTIVE -->|FP|SYNC| BP |<-- HACTIVE
 *       _______________________      _____________
 * |____|       VGA_HS          |____|
 */
   // Parameters for hcount
   parameter HACTIVE      = 11'd 1280,
             HFRONT_PORCH = 11'd 32,
             HSYNC        = 11'd 192,
             HBACK_PORCH  = 11'd 96,   
             HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC +
                            HBACK_PORCH; // 1600
   
   // Parameters for vcount
   parameter VACTIVE      = 10'd 480,
             VFRONT_PORCH = 10'd 10,
             VSYNC        = 10'd 2,
             VBACK_PORCH  = 10'd 33,
             VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC +
                            VBACK_PORCH; // 525

   logic endOfLine;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          hcount <= 0;
     else if (endOfLine) hcount <= 0;
     else                hcount <= hcount + 11'd 1;

   assign endOfLine = hcount == HTOTAL - 1;
       
   logic endOfField;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          vcount <= 0;
     else if (endOfLine)
       if (endOfField)   vcount <= 0;
       else              vcount <= vcount + 10'd 1;

   assign endOfField = vcount == VTOTAL - 1;

   // Horizontal sync: from 0x520 to 0x5DF (0x57F)
   // 101 0010 0000 to 101 1101 1111
   assign VGA_HS = !( (hcount[10:8] == 3'b101) &
                      !(hcount[7:5] == 3'b111));
   assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

   assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused
   
   // Horizontal active: 0 to 1279     Vertical active: 0 to 479
   // 101 0000 0000  1280              01 1110 0000  480
   // 110 0011 1111  1599              10 0000 1100  524
   assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
                        !( vcount[9] | (vcount[8:5] == 4'b1111) );

   /* VGA_CLK is 25 MHz
    *             __    __    __
    * clk50    __|  |__|  |__|
    *        
    *             _____       __
    * hcount[0]__|     |_____|
    */
   assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive
   
endmodule
