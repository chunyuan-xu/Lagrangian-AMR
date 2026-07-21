#pragma once
#include <cmath>

struct CDoubleVector {
	double x;
	double y;
	CDoubleVector() : x(0), y(0)
	{}
	CDoubleVector(double ax, double ay) : x(ax), y(ay)
	{}
	bool operator==(const CDoubleVector &a) const
	{
		return ((std::fabs(x - a.x) < 1e-100) && (std::fabs(y - a.y) < 1e-100));
	}
	bool operator!=(const CDoubleVector &a) const
	{
		return !(*this == a);
	}
	friend CDoubleVector operator+(const CDoubleVector &a, const CDoubleVector &b)
	{
		return CDoubleVector(a.x + b.x, a.y + b.y);
	}
	friend CDoubleVector operator--(const CDoubleVector &a)
	{
		return CDoubleVector(-a.x, -a.y);
	}
	friend CDoubleVector operator-(const CDoubleVector &a, const CDoubleVector &b)
	{
		return CDoubleVector(a.x - b.x, a.y - b.y);
	}
	friend CDoubleVector& operator+=(CDoubleVector &a, const CDoubleVector &b)
	{
		a.x += b.x;
		a.y += b.y;
		return a;
	}
	friend CDoubleVector& operator-=(CDoubleVector &a, const CDoubleVector &b)
	{
		a.x -= b.x;
		a.y -= b.y;
		return a;
	}
	friend CDoubleVector operator*(const CDoubleVector &a, double f)
	{
		return CDoubleVector(a.x * f, a.y * f);
	}
	friend CDoubleVector operator*(double f, const CDoubleVector &a)
	{
		return CDoubleVector(a.x * f, a.y * f);
	}
	friend CDoubleVector operator/(const CDoubleVector &a, double f)
	{
		return CDoubleVector(a.x / f, a.y / f);
	}

	friend double operator^(const CDoubleVector &a, const CDoubleVector &b)
	{
		return a.x * b.x + a.y * b.y;
	}
};

struct CDoubleMatrix {
	double xx;
	double xy;
	double yx;
	double yy;
	CDoubleMatrix() : xx(0), xy(0), yx(0), yy(0)
	{}
	CDoubleMatrix(double axx, double axy, double ayx, double ayy) :
		xx(axx), xy(axy), yx(ayx), yy(ayy)
	{}

	friend CDoubleMatrix operator--(const CDoubleMatrix &a)
	{
		return CDoubleMatrix(-a.xx, -a.xy, -a.yx, -a.yy);
	}
	friend CDoubleMatrix operator+(const CDoubleMatrix &a, const CDoubleMatrix &b)
	{
		return CDoubleMatrix(a.xx + b.xx, a.xy + b.xy, a.yx + b.yx, a.yy + b.yy);
	}
	friend CDoubleMatrix operator-(const CDoubleMatrix &a, const CDoubleMatrix &b)
	{
		return CDoubleMatrix(a.xx - b.xx, a.xy - b.xy, a.yx - b.yx, a.yy - b.yy);
	}

	friend CDoubleMatrix& operator+=(CDoubleMatrix &a, CDoubleMatrix &b)
	{
		a.xx += b.xx;
		a.xy += b.xy;
		a.yx += b.yx;
		a.yy += b.yy;
		return a;
	}
	friend CDoubleMatrix& operator-=(CDoubleMatrix &a, CDoubleMatrix &b)
	{
		a.xx -= b.xx;
		a.xy -= b.xy;
		a.yx -= b.yx;
		a.yy -= b.yy;
		return a;
	}

	friend CDoubleMatrix operator*(const CDoubleMatrix &a, double f)
	{
		return CDoubleMatrix(a.xx * f, a.xy * f, a.yx * f, a.yy * f);
	}
	friend CDoubleMatrix operator*(double f, const CDoubleMatrix &a)
	{
		return CDoubleMatrix(a.xx * f, a.xy * f, a.yx * f, a.yy * f);
	}
	friend CDoubleMatrix operator/(const CDoubleMatrix &a, double f)
	{
		return CDoubleMatrix(a.xx / f, a.xy / f, a.yx / f, a.yy / f);
	}
};
