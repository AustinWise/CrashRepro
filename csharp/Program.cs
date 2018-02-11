using System;
using System.Runtime.CompilerServices;

namespace ConsoleApp2
{
    class LoginViewModel
    {
        public string Email { get; set; }

        public bool RememberMe { get; set; }
    }
    delegate TResult MyFunc<in T, out TResult>(T arg);
    static class Program
    {
        static string GetEmail(LoginViewModel model)
        {
            return model.Email;
        }
        static bool GetRememberMe(LoginViewModel model)
        {
            return model.RememberMe;
        }

        static int Main(string[] args)
        {
            MyFunc<LoginViewModel, string> a = GetEmail;
            try
            {
                a(null);
            }
            catch
            {
            }
            MyFunc<LoginViewModel, bool> b = GetRememberMe;
            try
            {
                b(null);
            }
            catch
            {
            }

            Console.WriteLine("Still did not crash.");
            Console.Out.Flush();
            return 42;
        }
    }
}

