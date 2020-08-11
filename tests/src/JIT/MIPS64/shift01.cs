// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.
//

// Copyright (c) Loongson Technology. All rights reserved.
//

using System;

public class Test
{
    public static int Main()
    {
        int  result = 0;
        result += Test.AddKeyword("Session", (long)0x1 << 7);
        result += Test.AddKeyword("Session", (long)0x1 << 8);
        result += Test.AddKeyword("Session", (long)0x1 << 15);
        result += Test.AddKeyword("Session", (long)0x1 << 16);
        result += Test.AddKeyword("Session", (long)0x1 << 31);
        result += Test.AddKeyword("Session", (long)0x1 << 32);
        result += Test.AddKeyword("Session", (long)0x1 << 47);
        result += Test.AddKeyword("Session", (long)0x1 << 48);

        if (result == 8)
        {
            Console.WriteLine("Test Passed");
            return 100;
        }
        else
        {
            Console.WriteLine("Test Failed");
            return 1;
        }
    }

    public static int AddKeyword(string name, ulong value)
    {

        if ((value & (value - 1)) != 0)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }

}

