% example: guided feathering
% figure 9 in our paper

close all;

%I = double(imread('.\img_feathering\toy.bmp')) / 255;
%p = double(rgb2gray(imread('.\img_feathering\toy-mask.bmp'))) / 255;

I = double(imread('.\img_feathering\i3.png')) / 255;
%p = double(rgb2gray(imread('.\img_feathering\b.bmp'))) / 255;
p = double(imread('.\img_feathering\b.bmp')) / 255;


r = 60;
eps = 10^-6;

q = guidedfilter_color(I, p, r, eps);
imwrite(q, 'q.jpg');

figure();
imshow([I, repmat(p, [1, 1, 3]), repmat(q, [1, 1, 3])], [0, 1]);
 