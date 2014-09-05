% I = double(imread('i3.jpg')) / 255;
% p = double(imread('std_512.jpg')) / 255;
% 
% q = zeros(size(I));
% 
% r = 16;
% eps = 0.1^2;
% 
% q(:, :, 1) = guidedfilter(I(:, :, 1), p(:, :, 1), r, eps);
% q(:, :, 2) = guidedfilter(I(:, :, 2), p(:, :, 2), r, eps);
% q(:, :, 3) = guidedfilter(I(:, :, 3), p(:, :, 3), r, eps);
% 
% figure();
% imshow([I, q], [0, 1]);
% 
% return ;

function mytest() 

%I = double(imread('i3.jpg'));
%p = double(imread('std_512.jpg'));

p = double(imread('std_512.jpg'));
I = double(imread('com/0001.jpg'));


I_lab = rgb2lab(I(:, :, 1), I(:, :, 2), I(:, :, 3));
I_illum = I_lab(:, : , 1);
%imwrite(I_illum / 255, 'i3_illum.jpg');
%I_illum_ = norm(I_illum, 0.0, 1.0)
%I_illum_norm = I_illum / 255.0;
I_illum_norm = norm(I_illum, 0.0, 1.0);


p_lab = rgb2lab(p(:, :, 1), p(:, :, 2), p(:, :, 3));
p_illum = p_lab(:, : , 1);
%I_illum_ = norm(I_illum, 0.0, 1.0)
%p_illum_norm = p_illum / 255.0;
p_illum_norm = norm(p_illum, 0.0, 1.0);

r = 16;
%eps = 0.1^2;
eps = 0.01;

%q = guidedfilter(I_illum_norm, p_illum_norm, r, eps);

q = guidedfilter(I_illum_norm, I_illum_norm, r, eps);

figure;
%imshow(p_illum_norm);


%I_lab(:, :, 1) = q * 255.0;
max_I_illum = max(I_illum(:));
min_I_illum = min(I_illum(:));

q_res = q * (max_I_illum - min_I_illum) + min_I_illum;

%I_lab(:,:,1) = q;

detail = I_illum ./ q_res;



%I_lab(:, :, 1) = q * (max_I_illum - min_I_illum) + min_I_illum;

r = 30;
eps = 0.001;

q2 = guidedfilter(I_illum_norm, p_illum_norm, r, eps);

imshow([I_illum_norm p_illum_norm q q2], [0 1]);

qq = q2 * (max_I_illum - min_I_illum) + min_I_illum;

qq = qq .* detail;

I_lab(:,:,1) = qq;

dst = lab2rgb(I_lab);

figure, imshow(dst);


end

function dst = norm(A, l, r)
	A_max = max(A(:));
	A_min = min(A(:));
	%先转到0，1之间
	B = (A - A_min) / (A_max - A_min);
	dst = B * (r - l ) + l;
end