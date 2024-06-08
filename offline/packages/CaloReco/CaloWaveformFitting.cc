#include "CaloWaveformFitting.h"

#include <TF1.h>
#include <TFile.h>
#include <TProfile.h>
#include <TSpline.h>

#include <Fit/BinData.h>
#include <Fit/Chi2FCN.h>
#include <Fit/Fitter.h>
#include <Fit/UnBinData.h>
#include <HFitInterface.h>
#include <Math/WrappedMultiTF1.h>
#include <Math/WrappedTF1.h>
#include <ROOT/TThreadExecutor.hxx>
#include <ROOT/TThreadedObject.hxx>

#include <pthread.h>
#include <iostream>
#include <string>

ROOT::TThreadExecutor *t = new ROOT::TThreadExecutor(1);
double CaloWaveformFitting::template_function(double *x, double *par)
{
  Double_t v1 = par[0] * h_template->Interpolate(x[0] - par[1]) + par[2];
  return v1;
}

void CaloWaveformFitting::initialize_processing(const std::string &templatefile)
{
  TFile *fin = TFile::Open(templatefile.c_str());
  assert(fin);
  assert(fin->IsOpen());
  h_template = static_cast<TProfile *>(fin->Get("waveform_template"));
  m_peakTimeTemp = h_template->GetBinCenter(h_template->GetMaximumBin());
  t = new ROOT::TThreadExecutor(_nthreads);
}

std::vector<std::vector<float>> CaloWaveformFitting::process_waveform(std::vector<std::vector<float>> waveformvector)
{
  int size1 = waveformvector.size();
  std::vector<std::vector<float>> fitresults;
  for (int i = 0; i < size1; i++)
  {
    waveformvector.at(i).push_back(i);
  }
  fitresults = calo_processing_templatefit(waveformvector);
  return fitresults;
}

std::vector<std::vector<float>> CaloWaveformFitting::calo_processing_templatefit(std::vector<std::vector<float>> chnlvector)
{
  auto func = [&](std::vector<float> &v)
  {
    int size1 = v.size() - 1;
    if (size1 == _nzerosuppresssamples)
    {
      v.push_back(v.at(1) - v.at(0));  // returns peak sample - pedestal sample
      v.push_back(-1);                 // set time to -1 to indicate zero suppressed
      v.push_back(v.at(0));
      v.push_back(0);
    }
    else
    {
      float maxheight = 0;
      int maxbin = 0;
      for (int i = 0; i < size1; i++)
      {
        if (v.at(i) > maxheight)
        {
          maxheight = v.at(i);
          maxbin = i;
        }
      }
      float pedestal = 1500;
      if (maxbin > 4)
      {
        pedestal = 0.5 * (v.at(maxbin - 4) + v.at(maxbin - 5));
      }
      else if (maxbin > 3)
      {
        pedestal = (v.at(maxbin - 4));
      }
      else
      {
        pedestal = 0.5 * (v.at(size1 - 3) + v.at(size1 - 2));
      }

      if ( (_bdosoftwarezerosuppression && v.at(6) - v.at(0) < _nsoftwarezerosuppression) || (_maxsoftwarezerosuppression && maxheight-pedestal  < _nsoftwarezerosuppression)  )
      {
        v.push_back(v.at(6) - v.at(0));
        v.push_back(-1);
        v.push_back(v.at(0));
        v.push_back(0);
      }
      else
      {
        auto h = new TH1F(std::string("h_" + std::to_string((int) round(v.at(size1)))).c_str(), "", size1, -0.5, size1 - 0.5);
        for (int i = 0; i < size1; i++)
        {
          h->SetBinContent(i + 1, v.at(i));
          h->SetBinError(i + 1, 1);
        }
        auto f = new TF1(std::string("f_" + std::to_string((int) round(v.at(size1)))).c_str(), this, &CaloWaveformFitting::template_function, 0, 31, 3, "CaloWaveformFitting", "template_function");
        ROOT::Math::WrappedMultiTF1 *fitFunction = new ROOT::Math::WrappedMultiTF1(*f, 3);
        ROOT::Fit::BinData data(v.size() - 1, 1);
        ROOT::Fit::FillData(data, h);
        ROOT::Fit::Chi2Function *EPChi2 = new ROOT::Fit::Chi2Function(data, *fitFunction);
        ROOT::Fit::Fitter *fitter = new ROOT::Fit::Fitter();
        fitter->Config().MinimizerOptions().SetMinimizerType("GSLMultiFit");
        double params[] = {static_cast<double>(maxheight - pedestal), 0, static_cast<double>(pedestal)};
        fitter->Config().SetParamsSettings(3, params);
        fitter->Config().ParSettings(1).SetLimits(-1 * m_peakTimeTemp, size1 - m_peakTimeTemp);  // set lim on time par
        fitter->FitFCN(*EPChi2, nullptr, data.Size(), true);
        ROOT::Fit::FitResult fitres = fitter->Result();
        double chi2min = fitres.MinFcnValue();
        chi2min /= size1 - 3;  // divide by the number of dof
        for (int i = 0; i < 3; i++)
        {
          v.push_back(f->GetParameter(i));
        }

        v.push_back(chi2min);
        h->Delete();
        f->Delete();
        delete fitFunction;
        delete fitter;
        delete EPChi2;
      }
    }
  };

  t->Foreach(func, chnlvector);
  int size3 = chnlvector.size();
  std::vector<std::vector<float>> fit_params;
  std::vector<float> fit_params_tmp;
  for (int i = 0; i < size3; i++)
  {
    std::vector<float> tv = chnlvector.at(i);
    int size2 = tv.size();
    for (int q = 4; q > 0; q--)
    {
      fit_params_tmp.push_back(tv.at(size2 - q));
    }
    fit_params.push_back(fit_params_tmp);
    fit_params_tmp.clear();
  }
  chnlvector.clear();
  return fit_params;
}

void CaloWaveformFitting::FastMax(float x0, float x1, float x2, float y0, float y1, float y2, float &xmax, float &ymax)
{
  int n = 3;
  double xp[3] = {x0, x1, x2};
  double yp[3] = {y0, y1, y2};
  TSpline3 *sp = new TSpline3("", xp, yp, n, "b2e2", 0, 0);
  double X, Y, B, C, D;
  ymax = y1;
  xmax = x1;
  if (y0 > ymax)
  {
    ymax = y0;
    xmax = x0;
  }
  if (y2 > ymax)
  {
    ymax = y2;
    xmax = x2;
  }
  for (int i = 0; i <= 1; i++)
  {
    sp->GetCoeff(i, X, Y, B, C, D);
    if (D == 0)
    {
      if (C < 0)
      {
        // TSpline is a quadratic equation

        float root = -B / (2 * C) + X;
        if (root >= xp[i] && root <= xp[i + 1])
        {
          float yvalue = sp->Eval(root);
          if (yvalue > ymax)
          {
            ymax = yvalue;
            xmax = root;
          }
        }
      }
    }
    else
    {
      // find x when derivative = 0
      float root = (-2 * C + sqrt(4 * C * C - 12 * B * D)) / (6 * D) + X;
      if (root >= xp[i] && root <= xp[i + 1])
      {
        float yvalue = sp->Eval(root);
        if (yvalue > ymax)
        {
          ymax = yvalue;
          xmax = root;
        }
      }
      root = (-2 * C - sqrt(4 * C * C - 12 * B * D)) / (6 * D) + X;
      if (root >= xp[i] && root <= xp[i + 1])
      {
        float yvalue = sp->Eval(root);
        if (yvalue > ymax)
        {
          ymax = yvalue;
          xmax = root;
        }
      }
    }
  }
  delete sp;
  return;
}
std::vector<std::vector<float>> CaloWaveformFitting::calo_processing_fast(std::vector<std::vector<float>> chnlvector)
{
  std::vector<std::vector<float>> fit_values;
  int nchnls = chnlvector.size();
  for (int m = 0; m < nchnls; m++)
  {
    std::vector<float> v = chnlvector.at(m);
    int nsamples = v.size();

    double maxy = v.at(0);
    float amp = 0;
    float time = 0;
    float ped = 0;
    if (nsamples == 2)
    {
      amp = v.at(1);
      time = -1;
      ped = v.at(0);
    }
    else if (nsamples >= 3)
    {
      int maxx = 0;
      for (int i = 0; i < nsamples; i++)
      {
        if (i < 3)
        {
          ped += v.at(i);
        }
        if (v.at(i) > maxy)
        {
          maxy = v.at(i);
          maxx = i;
        }
      }
      ped /= 3;
      // if maxx <=5 nsample >=10 use the last two sample for pedestal(for HCal TP)
      if (maxx <= 5 && nsamples >= 10)
      {
        ped = 0.5 * (v.at(nsamples - 2) + v.at(nsamples - 1));
      }
      if (maxx == 0 || maxx == nsamples - 1)
      {
        amp = maxy;
        time = maxx;
      }
      else
      {
        FastMax(maxx - 1, maxx, maxx + 1, v.at(maxx - 1), v.at(maxx), v.at(maxx + 1), time, amp);
      }
    }
    amp -= ped;
    std::vector<float> val = {amp, time, ped, 0};
    fit_values.push_back(val);
    val.clear();
  }
  return fit_values;
}

std::vector<std::vector<float>> CaloWaveformFitting::calo_processing_nyquist(std::vector<std::vector<float>> chnlvector)
{
  std::vector<std::vector<float>> fit_values;
  int nchnls = chnlvector.size();
  for (int m = 0; m < nchnls; m++)
  {
    std::vector<float> v = chnlvector.at(m);
    int nsamples = (int)v.size();

    if (nsamples == 2)
    {
      fit_values.push_back({v.at(1) - v.at(0), -1, v.at(0), 0});
      continue;
    }
    
    std::vector<float> result = NyquistInterpolation(v);
    fit_values.push_back(result);
  }
  return fit_values;

}

std::vector<float> CaloWaveformFitting::NyquistInterpolation(std::vector<float> &vec_signal_samples)
{
  // int N = (int) vec_signal_samples.size();

  auto max_elem_iter = std::max_element(vec_signal_samples.begin(), vec_signal_samples.end());
  int maxx = std::distance(vec_signal_samples.begin(), max_elem_iter);
  float max = *max_elem_iter;

  float maxpos = 0;
  max = 0;
  for (float i = maxx - 1; i < maxx + 1; i += 0.01)
  {
    float yval = psinc(i, vec_signal_samples);
    if (yval > max)
    {
      max = yval;
      maxpos = i;
    }
  }

  float pedestal = 0;

  if (maxpos > 5)
  {
    for (int i = 0; i < 3; i++)
    {
      pedestal += vec_signal_samples[i];
    }
    pedestal = pedestal / 3;
  }
  else if (maxpos > 4)
  {
    pedestal = (vec_signal_samples[0] + vec_signal_samples[1]) / 2;
  }
  else
  {
    pedestal = max;
    for (float i = maxpos - 5; i < maxpos; i += 0.1)
    {
      float yval = psinc(i, vec_signal_samples);
      if (yval < pedestal)
      {
        pedestal = yval;
      }
    }
  }
  //calculate chi2 using the tempalte
  float chi2 = 0;
  double par[3] = {max - pedestal, maxpos - m_peakTimeTemp, pedestal};
  for(int i = 0; i < (int)vec_signal_samples.size(); i++){
    double xval[1] = {(double)i};
    float diff = vec_signal_samples[i] - template_function(xval, par);
    chi2 += diff*diff;
  }
  std::vector<float> val = {max - pedestal, maxpos, pedestal, chi2};
  return val;
}

// for odd N
double CaloWaveformFitting::Dkernelodd(double x, int N)
{
  double sum = 0;
  for (int k = 0; k < (N + 1) / 2; k++)
  {
    sum += 2 * std::cos(2 * M_PI * k * x / N);
  }
  sum -= 1;
  sum = sum / N;
  return sum;
}

// for even N
double CaloWaveformFitting::Dkernel(double x, int N)
{
  double sum = 0;
  for (int k = 0; k < N / 2; k++)
  {
    sum += 2 * std::cos(2 * M_PI * k * x / N);
  }
  sum -= 1;
  sum += std::cos(M_PI * x);
  sum = sum / N;
  return sum;
}

float CaloWaveformFitting::stablepsinc(float time, std::vector<float> &vec_signal_samples)
{
  int N = (int) vec_signal_samples.size();
  float sum = 0;
  if (N % 2 == 0)
  {
    for (int n = 0; n < N; n++)
    {
      sum += vec_signal_samples[n] * Dkernel(time - n, N);
    }
  }
  else
  {
    for (int n = 0; n < N; n++)
    {
      sum += vec_signal_samples[n] * Dkernelodd(time - n, N);
    }
  }
  return sum;
}

float CaloWaveformFitting::psinc(float time, std::vector<float> &vec_signal_samples)
{
  int N = (int) vec_signal_samples.size();

  if ((std::round(time) - time) < 1e-6)
  {
    return stablepsinc(time, vec_signal_samples);
  }

  float sum = 0;
  if (N % 2 == 0)
  {
    for (int n = 0; n < N; n++)
    {
      double piu = M_PI * (time - n);
      double piuN = piu / N;
      sum += vec_signal_samples[n] * std::sin(piu) / (std::tan(piuN)) / N;
    }
  }
  else
  {
    for (int n = 0; n < N; n++)
    {
      double piu = M_PI * (time - n);
      double piuN = piu / N;
      sum += vec_signal_samples[n] * std::sin(piu) / (std::sin(piuN)) / N;
    }
  }
  return sum;
}