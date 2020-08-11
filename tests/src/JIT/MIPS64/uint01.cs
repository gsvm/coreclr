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
        uint value = 0xF0000000;

        if (value < 0xFFFFFFFF)
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

}

